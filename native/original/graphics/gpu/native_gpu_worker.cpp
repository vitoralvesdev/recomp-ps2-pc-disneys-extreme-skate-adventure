// Disney: ordered, bounded native OpenGL command worker. No instruction interpreter.
#include "gs_hw_backend.h"
#include "native_gpu_capture.h"
#include "native_gpu_stats.h"
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <vector>
#include <xmmintrin.h>

namespace {
// One queue entry for both draw batches and control calls. Batches carry the payload inline: the geometric
// path submits one batch per GS primitive, and boxing it in std::function heap-allocated on every triangle.
struct GpuOp {
    enum class Kind : uint8_t { Call, BatchRange };
    Kind kind = Kind::Call;
    unsigned csr = 0;
    std::vector<GSPrimitiveBatch> batches;
    std::function<void()> fn;
};

class DisneyGpuWorker final : public GSRasterBackend {
    mutable std::mutex mutex;
    mutable std::condition_variable ready,space;
    mutable std::deque<GpuOp> queue;
    mutable std::exception_ptr failure;
    bool stopping=false;
    std::unique_ptr<GSHwBackend> backend;
    DisneyCapture::Writer capture;
    std::thread worker;
    // Raised from 512 to 2048 so that a heavy frame (level load + many texture
    // uploads) does not stall the EE game thread while the GPU worker drains.
    // The game thread blocks only if the worker falls more than 2048 commands
    // behind, which is not reachable at normal play cadence.
    static constexpr size_t kQueueLimit = 2048u;
    // The geometric path submits one batch per GS primitive (~31k per gameplay frame). Locking, allocating a deque
    // node and waking the worker for each of them cost more than the draw itself, so the batches are staged and
    // handed over in chunks: one lock, one node and one wake per chunk.
    static constexpr size_t kBatchChunk = 256u;
    static constexpr size_t kQueueBatchLimit = 8192u;
    mutable std::vector<GSPrimitiveBatch> stagedBatches;
    mutable size_t stagedCsr = 0;
    mutable size_t queuedBatches = 0;

    void flushStagedLocked(std::unique_lock<std::mutex>& lock) const {
        if(stagedBatches.empty())return;
        if(queuedBatches + stagedBatches.size() > kQueueBatchLimit){
            DisneyGpuStats::Timer<decltype(DisneyGpuStats::queueBlockNs)> blockTimer(DisneyGpuStats::queueBlockNs);
            ++DisneyGpuStats::queueBlocks;
            space.wait(lock,[&]{return queuedBatches + stagedBatches.size() <= kQueueBatchLimit || stopping;});
        }
        if(stopping)throw std::runtime_error("Native GPU worker has stopped");
        GpuOp op;op.kind=GpuOp::Kind::BatchRange;op.csr=static_cast<unsigned>(stagedCsr);
        op.batches.swap(stagedBatches);
        queuedBatches+=op.batches.size();
        queue.emplace_back(std::move(op));ready.notify_one();
    }
    void enqueue(GpuOp&& op) const {
        std::unique_lock lock(mutex);
        flushStagedLocked(lock);
        if(queue.size()>=kQueueLimit){
            DisneyGpuStats::Timer<decltype(DisneyGpuStats::queueBlockNs)> blockTimer(DisneyGpuStats::queueBlockNs);
            ++DisneyGpuStats::queueBlocks;
            space.wait(lock,[&]{return queue.size()<kQueueLimit || stopping;});
        }
        if(stopping)throw std::runtime_error("Native GPU worker has stopped");
        queue.emplace_back(std::move(op));ready.notify_one();
    }
    void post(std::function<void()> fn) const { GpuOp op;op.fn=std::move(fn);enqueue(std::move(op)); }
    template<class F> auto call(F fn) const {
        DisneyGpuStats::Timer<decltype(DisneyGpuStats::syncWaitNs)> syncTimer(DisneyGpuStats::syncWaitNs);
        ++DisneyGpuStats::syncCalls;
        using Result=std::invoke_result_t<F>;
        auto task=std::make_shared<std::packaged_task<Result()>>(std::move(fn));
        auto result=task->get_future();
        GpuOp op;op.fn=[task]{(*task)();};enqueue(std::move(op));
        if constexpr(std::is_void_v<Result>) {
            result.get();std::lock_guard lock(mutex);if(failure)std::rethrow_exception(failure);
        } else {
            auto value=result.get();std::lock_guard lock(mutex);if(failure)std::rethrow_exception(failure);return value;
        }
    }
    void run() {
        backend=std::make_unique<GSHwBackend>();
        for(;;) {
            std::deque<GpuOp> work;
            {
                std::unique_lock lock(mutex);ready.wait(lock,[&]{return stopping || !queue.empty();});
                if(stopping && queue.empty())break;
                work.swap(queue);space.notify_all();
            }
            DisneyGpuStats::Timer<decltype(DisneyGpuStats::workerBusyNs)> busyTimer(DisneyGpuStats::workerBusyNs);
            for(auto& op:work)try {
                if(op.kind==GpuOp::Kind::BatchRange){
                    const auto old=_mm_getcsr();_mm_setcsr(op.csr);
                    std::exception_ptr error;
                    try {
                        for(auto& batch:op.batches){
                            capture.record(DisneyCapture::Submit,batch);
                            backend->Submit(batch);
                        }
                    }catch(...){error=std::current_exception();}
                    _mm_setcsr(old);
                    {
                        std::lock_guard lock(mutex);queuedBatches-=op.batches.size();
                    }
                    space.notify_all();
                    if(error)std::rethrow_exception(error);
                } else op.fn();
            }catch(...) {std::lock_guard lock(mutex);if(!failure)failure=std::current_exception();}
        }
        // The same thread creates, uses and destroys the WGL context.
        backend.reset();
    }
public:
    DisneyGpuWorker():worker([this]{run();}){}
    ~DisneyGpuWorker() override {
        try {call([]{});}catch(...){}
        {std::lock_guard lock(mutex);stopping=true;}ready.notify_all();space.notify_all();worker.join();
    }
    void Initialize(uint8_t* v,uint32_t size) override {call([=,this]{capture.record(DisneyCapture::Init,v,size);backend->Initialize(v,size);});}
    void Reset() override {call([this]{capture.record(DisneyCapture::Reset);backend->Reset();});}
    void Submit(const GSPrimitiveBatch& batch) override {
        if(batch.vertexCount==0u)return;
        std::unique_lock lock(mutex);
        if(stagedBatches.empty())stagedCsr=_mm_getcsr();
        stagedBatches.push_back(batch);
        if(stagedBatches.size()>=kBatchChunk)flushStagedLocked(lock);
    }
    void LoadClut(const GSTex0Reg& t,const GSTexClutReg& c) override {post([=,this]{capture.record(DisneyCapture::Clut,DisneyCapture::ClutData{t,c});backend->LoadClut(t,c);});}
    void BeginTransfer(const GSTransferCommand& c) override {post([=,this]{capture.record(DisneyCapture::Transfer,c);backend->BeginTransfer(c);});}
    void UploadImage(const uint8_t* data,uint32_t size) override {
        if(!data || !size)return;
        // Queued commands own their payload; caller memory can be reused immediately.
        std::vector<uint8_t> bytes(data,data+size);
        post([this,bytes=std::move(bytes)]{capture.record(DisneyCapture::Upload,bytes.data(),static_cast<uint32_t>(bytes.size()));backend->UploadImage(bytes.data(),static_cast<uint32_t>(bytes.size()));});
    }
    void Flush() override {
        DisneyGpuStats::Timer<decltype(DisneyGpuStats::flushWaitNs)> flushTimer(DisneyGpuStats::flushWaitNs);
        call([this]{capture.record(DisneyCapture::Flush);backend->Flush();});
    }
    void TextureFlush() override {post([this]{capture.record(DisneyCapture::TexFlush);backend->TextureFlush();});}
    void Sync(GSSyncReason reason) override {
        DisneyGpuStats::Timer<decltype(DisneyGpuStats::syncOpWaitNs)> syncTimer(DisneyGpuStats::syncOpWaitNs);
        call([=,this]{capture.record(DisneyCapture::Sync,reason);backend->Sync(reason);});
    }
    PresentationFrame Present(const GSPresentationRequest& request) override {
        DisneyGpuStats::Timer<decltype(DisneyGpuStats::presentWaitNs)> presentTimer(DisneyGpuStats::presentWaitNs);
        ++DisneyGpuStats::presentCalls;
        return call([=,this]{capture.record(DisneyCapture::Present,request);return backend->Present(request);});
    }
    bool ClearFramebuffer(const GSContext& c,uint32_t rgba) override {
        DisneyGpuStats::Timer<decltype(DisneyGpuStats::clearWaitNs)> clearTimer(DisneyGpuStats::clearWaitNs);
        return call([=,this]{capture.record(DisneyCapture::Clear,DisneyCapture::ClearData{c,rgba});return backend->ClearFramebuffer(c,rgba);});
    }
    uint32_t ConsumeLocalToHostBytes(uint8_t* out,uint32_t count) override {return call([=,this]{return backend->ConsumeLocalToHostBytes(out,count);});}
    uint32_t ReadVram(uint32_t p,uint32_t b,uint32_t w,uint32_t x,uint32_t y) const override {
        DisneyGpuStats::Timer<decltype(DisneyGpuStats::readWaitNs)> readTimer(DisneyGpuStats::readWaitNs);
        return call([=,this]{return backend->ReadVram(p,b,w,x,y);});
    }
    void WriteVram(uint32_t p,uint32_t b,uint32_t w,uint32_t x,uint32_t y,uint32_t value) override {post([=,this]{capture.record(DisneyCapture::Write,DisneyCapture::WriteData{p,b,w,x,y,value});backend->WriteVram(p,b,w,x,y,value);});}
    void SnapshotVram(std::vector<uint8_t>& out) const override {call([&,this]{backend->SnapshotVram(out);});}
    GSTransferSnapshot GetTransferSnapshot() const override {return call([this]{return backend->GetTransferSnapshot();});}
};
}
std::unique_ptr<GSRasterBackend> makeDisneyNativeGpuBackend() {return std::make_unique<DisneyGpuWorker>();}
