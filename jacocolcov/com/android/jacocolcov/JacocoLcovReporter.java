/*
 * Copyright 2022 The Android Open Source Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.jacocolcov;

import static org.jacoco.core.analysis.ICounter.EMPTY;
import static org.jacoco.core.analysis.ISourceNode.UNKNOWN_LINE;

import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.CommandLineParser;
import org.apache.commons.cli.Option;
import org.apache.commons.cli.OptionBuilder;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;
import org.apache.commons.cli.PosixParser;
import org.jacoco.core.analysis.Analyzer;
import org.jacoco.core.analysis.CoverageBuilder;
import org.jacoco.core.analysis.IClassCoverage;
import org.jacoco.core.analysis.ILine;
import org.jacoco.core.analysis.IMethodCoverage;
import org.jacoco.core.tools.ExecFileLoader;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/** Converts JaCoCo measurements and classfiles into a lcov-format coverage report. */
final class JacocoLcovReporter {

    public static void main(String[] args) {
        Options options = new Options();

        Option classfilesOption =
                OptionBuilder.hasArg()
                        .withArgName("<path>")
                        .withDescription("location of the Java class files")
                        .create("classfiles");

        Option sourcepathOption =
                OptionBuilder.hasArg()
                        .withArgName("<path>")
                        .withDescription("location of the source files")
                        .create("sourcepath");

        Option outputOption =
                OptionBuilder.isRequired()
                        .hasArg()
                        .withArgName("<destfile>")
                        .withDescription("location to write lcov data")
                        .create("o");

        Option verboseOption = OptionBuilder.withDescription("verbose logging").create("v");

        Option strictOption =
                OptionBuilder.withDescription("fail if any error is encountered").create("strict");

        options.addOption(classfilesOption);
        options.addOption(sourcepathOption);
        options.addOption(outputOption);
        options.addOption(verboseOption);
        options.addOption(strictOption);

        CommandLineParser parser = new PosixParser();
        CommandLine cmd;

        try {
            cmd = parser.parse(options, args);
        } catch (ParseException e) {
            System.err.println("error parsing command line options: " + e.getMessage());
            System.exit(1);
        }

        String[] classfiles = cmd.getOptionValues("classfiles");
        String[] sourcepaths = cmd.getOptionValues("sourcepath");
        String outputFile = cmd.getOptionValue("o");
        boolean verbose = cmd.hasOption("v");
        boolean strict = cmd.hasOption("strict");
        String[] execfiles = cmd.getArgs();

        JacocoLcovReporter reporter = new JacocoLcovReporter(verbose, strict);

        for (String sourcepath : sourcepaths) {
            reporter.addSourcePath(new File(sourcepath));
        }

        try {
            for (String execfile : execfiles) {
                reporter.loadExecfile(new File(execfile));
            }

            for (String classfile : classfiles) {
                reporter.loadClassfile(new File(classfile));
            }

            reporter.write(new File(outputFile));
        } catch (Exception e) {
            System.err.println("Failed to generate a coverage report: " + e.getMessage());
            System.exit(2);
        }
    }

    private Analyzer analyzer;
    private CoverageBuilder builder;
    private ExecFileLoader loader;

    private int execfilesLoaded;
    private int classfilesLoaded;
    private Map<String, Set<String>> sourceFiles;

    private final boolean verbose;
    private final boolean strict;

    public JacocoLcovReporter(final boolean verbose, final boolean strict) {
        this.verbose = verbose;
        this.strict = strict;
        reset();
    }

    /** Resets the reporter to the empty state. */
    public void reset() {
        analyzer = null;
        builder = new CoverageBuilder();
        loader = new ExecFileLoader();
        execfilesLoaded = 0;
        classfilesLoaded = 0;
        sourceFiles = new HashMap<>();
    }

    /**
     * Searches the path for Java or Kotlin files.
     *
     * @param path the path to search for files
     */
    public void addSourcePath(File path) {
        // Recursively add all .java or .kt files found to the list of source files.
        // The sourceFiles map contains <filename> -> set of <absolute paths>.
        for (File file : path.listFiles()) {
            if (file.isDirectory()) {
                addSourcePath(file);
            } else {
                String name = file.getName();
                if (name.endsWith(".java") || name.endsWith(".kt")) {
                    if (verbose) {
                        System.out.println("Found source file " + file.getAbsolutePath());
                    }
                    sourceFiles.putIfAbsent(name, new HashSet<>());
                    sourceFiles.get(name).add(file.getAbsolutePath());
                }
            }
        }
    }

    /**
     * Loads JaCoCo execution data files.
     *
     * <p>If strict is not set, logs any exception thrown and returns. If strict is set, rethrows
     * any exception encountered while loading the file.
     *
     * @param execfile the file to load
     * @throws IOException on error reading file or incorrect file format
     */
    public void loadExecfile(final File execfile) throws IOException {
        try {
            if (verbose) {
                System.out.println("Loading execfile " + execfile.getPath());
            }
            loader.load(execfile);
            execfilesLoaded++;
        } catch (IOException e) {
            System.err.println("Failed to load execfile " + execfile.getPath());
            if (strict) {
                throw e;
            }
            System.err.println(e.getMessage());
        }
    }

    /**
     * Loads uninstrumented Java classfiles.
     *
     * <p>This should be run only after loading all execfiles, otherwise coverage data may be
     * incorrect. If strict is not set, logs any exception thrown and returns. If strict is set,
     * rethrows any exception encountered while loading the file.
     *
     * @param classfile the classfile or classfile archive to load
     * @throws IOException on error reading file or incorrect file format
     */
    public void loadClassfile(final File classfile) throws IOException {
        if (analyzer == null) {
            analyzer = new Analyzer(loader.getExecutionDataStore(), builder);
        }

        try {
            if (verbose) {
                System.out.println("Loading classfile " + classfile.getPath());
            }
            analyzer.analyzeAll(classfile);
            classfilesLoaded++;
        } catch (IOException e) {
            System.err.println("Failed to load classfile " + classfile.getPath());
            if (strict) {
                throw e;
            }
            System.err.println(e.getMessage());
        }
    }

    /**
     * Writes out the lcov format file based on the exec data and classfiles loaded.
     *
     * @param outputFile the file to write to
     * @throws IOException on error writing to the output file
     */
    public void write(final File outputFile) throws IOException {
        if (verbose) {
            System.out.println(
                    execfilesLoaded
                            + " execfiles loaded and "
                            + classfilesLoaded
                            + " classfiles loaded.");
        }

        try (FileWriter writer = new FileWriter(outputFile)) {
            // Write lcov header test name: <test name>. Displayed on the front page but otherwise
            // not used for anything important.
            writer.write("TN:" + outputFile.getName() + "\n");

            for (IClassCoverage coverage : builder.getClasses()) {
                if (coverage.isNoMatch()) {
                    String message = "Mismatch in coverage data for " + coverage.getName();
                    if (verbose) {
                        System.out.println(message);
                    }
                    if (strict) {
                        throw new IOException(message);
                    }
                }
                // Looping over coverage.getMethods() is done multiple times below due to lcov
                // ordering requirements.
                // lcov was designed around native code, and uses functions rather than methods as
                // its terminology of choice. We use methods here as we are working with Java code.
                int methodsFound = 0;
                int methodsHit = 0;
                int linesFound = 0;
                int linesHit = 0;

                // Sourcefile information: <absolute path to sourcefile>. If the sourcefile does not
                // match any file given on --sourcepath, it will not be included in the coverage
                // report.
                String sourcefile = findSourceFileMatching(sourcefile(coverage));
                if (sourcefile == null) {
                    continue;
                }
                writer.write("SF:" + sourcefile + "\n");

                // Function information: <starting line>,<name>.
                for (IMethodCoverage method : coverage.getMethods()) {
                    writer.write("FN:" + method.getFirstLine() + "," + name(method) + "\n");
                }

                // Function coverage information: <execution count>,<name>.
                for (IMethodCoverage method : coverage.getMethods()) {
                    int count = method.getMethodCounter().getCoveredCount();
                    writer.write("FNDA:" + count + "," + name(method) + "\n");

                    methodsFound++;
                    if (count > 0) {
                        methodsHit++;
                    }
                }

                // Write the count of methods(functions) found and hit.
                writer.write("FNF:" + methodsFound + "\n");
                writer.write("FNH:" + methodsHit + "\n");

                // TODO: Write branch coverage information.

                // Write line coverage information.
                for (IMethodCoverage method : coverage.getMethods()) {
                    int start = method.getFirstLine();
                    int end = method.getLastLine();

                    if (start == UNKNOWN_LINE || end == UNKNOWN_LINE) {
                        continue;
                    }

                    for (int i = start; i <= end; i++) {
                        ILine line = method.getLine(i);
                        if (line.getStatus() == EMPTY) {
                            continue;
                        }
                        int count = line.getInstructionCounter().getCoveredCount();
                        writer.write("DA:" + i + "," + count + "\n");

                        linesFound++;
                        if (count > 0) {
                            linesHit++;
                        }
                    }
                }

                // Write the count of lines hit and found.
                writer.write("LH:" + linesHit + "\n");
                writer.write("LF:" + linesFound + "\n");

                // End of the sourcefile block.
                writer.write("end_of_record\n");
            }
        } catch (IOException e) {
            System.err.println("Failed to write to " + outputFile.getPath());
            System.err.println(e.getMessage());
            throw e;
        }

        System.out.println("Coverage data written to " + outputFile.getPath());
    }

    /**
     * Finds the sourcefile that maches the given filename.
     *
     * <p>Searches all the files indexed on -sourcepath and returns the first file that matches the
     * package and class name.
     *
     * @param filename the filename to match
     * @return the absolute path to the file, or null if none was found
     */
    private String findSourceFileMatching(String filename) {
        String key = new File(filename).getName();
        for (String absPath : sourceFiles.getOrDefault(key, new HashSet<>())) {
            if (absPath.endsWith(filename)) {
                if (verbose) {
                    System.out.println(filename + " matched to " + absPath);
                }
                return absPath;
            }
        }
        if (verbose) {
            System.out.println(filename + " did not match any source path");
        }
        return null;
    }

    /** Utility function to convert IClassCoverage to a sourcefile name. */
    private String sourcefile(IClassCoverage coverage) {
        return coverage.getPackageName() + "/" + coverage.getSourceFileName();
    }

    /** Utility function to convert IMethodCoverage to a unique method descriptor. */
    private String name(IMethodCoverage coverage) {
        return coverage.getName() + coverage.getDesc();
    }
}
