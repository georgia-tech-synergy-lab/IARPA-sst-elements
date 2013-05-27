package gov.sandia.sst.oberon.compiler;

import gov.sandia.sst.oberon.compiler.exp.OberonExpressionException;
import gov.sandia.sst.oberon.compiler.stmt.FunctionDefinition;
import gov.sandia.sst.oberon.compiler.stmt.OberonStatementException;
import gov.sandia.sst.oberon.compiler.visitor.OberonASTPrinter;

import java.io.*;
import java.util.*;

public class OberonCompiler {

	public static void main(String[] args) {
		OberonCompilerOptions.getInstance().processOptions(args);
		
		Vector<String> compileThese = OberonCompilerOptions.getInstance().getFilesForCompile();
		Vector<FunctionDefinition> functions = new Vector<FunctionDefinition>();
		Vector<FunctionDefinition> thisUnitFunctions;
		
		for(String nextFile : compileThese) {
			try {
				OberonParser parser = new OberonParser(new FileInputStream(nextFile));
				
				parser.setFileName(nextFile);
				thisUnitFunctions = parser.MultiFunctionFile();
				
				functions.addAll(thisUnitFunctions);
			} catch (FileNotFoundException e) {
				System.err.println("File: " + nextFile + " not found.");
				e.printStackTrace();
				System.exit(-1);
			} catch (ParseException e) {
				System.err.println("Compile failed for: " + nextFile);
				System.err.println(e.getMessage());
				e.printStackTrace();
				System.exit(-1);
			}
		}
		
		System.out.println("Found: " + functions.size() + " functions during compile");
		
		for(FunctionDefinition nextFunc : functions) {
			System.out.println("> " + nextFunc.getFunctionName() + " (w/" +
				nextFunc.getParameters().size() + " parameters)");
		}
		
		if(OberonCompilerOptions.getInstance().dumpASTToConsole()) {
			OberonASTPrinter printer = new OberonASTPrinter(System.out);
			
			for(FunctionDefinition nextFunc : functions) {
				try {
					printer.visit(nextFunc);
				} catch (OberonStatementException e) {
					System.err.println("Error (Statement Parsing): " +
							e.getFileName() + " at: " +
							e.getLineNumber() + " col: " +
							e.getColumnNumber());
					System.err.println("Error: " + e.getMessage());
					e.printStackTrace();
					System.exit(-1);
				} catch ( OberonExpressionException e) {
					System.err.println("Error (Expression Parsing): " +
							e.getFileName() + " at: " +
							e.getLineNumber() + " col: " +
							e.getColumnNumber());
					System.err.println("Error: " + e.getMessage());
					e.printStackTrace();
					System.exit(-1);
				}
			}
		}
	}

}
