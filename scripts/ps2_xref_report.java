import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.program.model.listing.Function;
import java.io.*;
import java.util.*;
public class ps2_xref_report extends GhidraScript {
 public void run() throws Exception {
  try (PrintWriter out = new PrintWriter(new File(getScriptArgs()[0]), "UTF-8")) {
   var refs = currentProgram.getReferenceManager().getReferencesTo(toAddr(Long.decode(getScriptArgs()[1])));
   Set<Long> found = new HashSet<>();
   DecompInterface decompiler = new DecompInterface();
   try { decompiler.openProgram(currentProgram);
    while (refs.hasNext()) {
     var reference = refs.next();
     Function function = getFunctionContaining(reference.getFromAddress());
     out.println("Reference: " + reference + " Function: " + function);
     if (function != null && found.add(function.getEntryPoint().getOffset())) {
      var result = decompiler.decompileFunction(function, 60, monitor);
      if (result.decompileCompleted()) out.println(result.getDecompiledFunction().getC());
     }
    }
   } finally { decompiler.dispose(); }
  }
 }
}
