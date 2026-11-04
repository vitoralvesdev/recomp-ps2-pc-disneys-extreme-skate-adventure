import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import java.io.*;
public class ps2_audio_report extends GhidraScript {
 public void run() throws Exception {
  try(PrintWriter out=new PrintWriter(new File(getScriptArgs()[0]),"UTF-8")) {
   DecompInterface d=new DecompInterface();
   try { d.openProgram(currentProgram);
    long start=getScriptArgs().length>1?Long.decode(getScriptArgs()[1]):0x13e500;
    long end=getScriptArgs().length>2?Long.decode(getScriptArgs()[2]):0x13f140;
    var fs=currentProgram.getFunctionManager().getFunctions(toAddr(start),true);
    while(fs.hasNext()) { var f=fs.next(); if(f.getEntryPoint().getOffset()>=end) break;
     if(!f.getName().startsWith("FUN_")) continue;
     out.println("\n// "+f.getEntryPoint()+" "+f.getName());
     var r=d.decompileFunction(f,60,monitor);
     if(r.decompileCompleted()) out.println(r.getDecompiledFunction().getC());
    }
   } finally {d.dispose();}
  }
 }
}
