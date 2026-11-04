// Regression from Disney 0x15a548 -> GS 0x15a690 -> 0x15a614:
// a ready thread must not run in the middle of a native interrupt invocation.
static int atomicIrqStage=0,atomicIrqChild=0;
static void atomicIrqChildEntry(uint8_t*,R5900Context* ctx,PS2Runtime*) {
 check(atomicIrqStage==2,"interrupt completes before a higher-priority ready thread runs");
 atomicIrqStage=3;ctx->pc=0;
}
static void atomicIrqFirst(uint8_t*,R5900Context* ctx,PS2Runtime* rt) {
 atomicIrqStage=1;
 check(rt->eeScheduler().startThread(atomicIrqChild,0,*ctx,true)==0,"wake competing thread inside interrupt");
 ctx->pc=0x102cu;
}
static void atomicIrqSecond(uint8_t*,R5900Context* ctx,PS2Runtime*) {
 check(atomicIrqStage==1,"interrupt continuation preserves critical section");
 atomicIrqStage=2;ctx->pc=getRegU32(ctx,31);
}
static void atomicIrqMain(uint8_t*,R5900Context* ctx,PS2Runtime* rt) {
 int old=0;check(rt->eeScheduler().changePriority(1,20,false,old)==0,"set main priority");
 EeThreadCreateParams params{};params.entry=0x1030;params.stack=0x1ff0000;params.stackSize=0x4000;params.priority=10;
 atomicIrqChild=rt->eeScheduler().createThread(params);check(atomicIrqChild>0,"create interrupt competitor");
 rt->eeScheduler().addIrqHandler(true,2,0x1028,false,0,0,0);
 ctx->pc=0x1034;rt->eeScheduler().dispatchIrq(true,2);
}
static void atomicIrqFinished(uint8_t*,R5900Context*,PS2Runtime* rt){
 check(atomicIrqStage==3,"competing thread eventually resumes after interrupt");
 rt->eeScheduler().requestStop();
}
static void testInterruptAtomicity(PS2Runtime& rt,std::vector<uint8_t>& ram){
 atomicIrqStage=0;
 rt.registerFunction(0x1024,&atomicIrqMain);rt.registerFunction(0x1028,&atomicIrqFirst);rt.registerFunction(0x102c,&atomicIrqSecond);rt.registerFunction(0x1030,&atomicIrqChildEntry);rt.registerFunction(0x1034,&atomicIrqFinished);
 R5900Context c{};c.pc=0x1024;rt.eeScheduler().reset(ram.data(),c);rt.eeScheduler().run();
 std::puts("PASS: IRQ critical section survives higher-priority wakeup; deferred thread resumes");
}

