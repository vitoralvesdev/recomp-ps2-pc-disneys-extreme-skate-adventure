// @category PS2Port
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.program.model.listing.Function;
import java.io.*;
public class ps2_function_report extends GhidraScript {
 public void run() throws Exception {
  File output = new File(getScriptArgs()[0]);
  try (PrintWriter out = new PrintWriter(output, "UTF-8")) {
   out.println("Program: " + currentProgram.getName());
   out.println("Language: " + currentProgram.getLanguageID());
   out.println("Functions: " + currentProgram.getFunctionManager().getFunctionCount());
   Function entry = getFunctionContaining(toAddr(Long.decode(getScriptArgs()[1])));
   if (entry != null) {
    DecompInterface decompiler = new DecompInterface();
    try {
     decompiler.openProgram(currentProgram);
     var result = decompiler.decompileFunction(entry, 60, monitor);
     if (result.decompileCompleted()) out.println(result.getDecompiledFunction().getC());
     else out.println("Decompilation incomplete: " + result.getErrorMessage());
    } finally { decompiler.dispose(); }
   }
  }
 }
}


