//
// Utility to build a segment from the provided assembler
// source files, placing routines where they need to live
//

#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>

#if defined(WIN32) || defined(_WIN32) 
    #define DIR_SEPARATOR "\\" 
#else 
    #define DIR_SEPARATOR "/" 
#endif 

const std::string ASM_CMD       = "java -jar assembler/KickAss.jar ";
const std::string LAB_OUT_START = "__routine_START_";
const std::string LAB_OUT_END   = "__routine_END_";
const std::string LAB_IN_START  = ".label __routine_START_";
const std::string LAB_IN_END    = ".label __routine_END_";

const std::string BANNER_LINE   = "//-------------------------------------------------------------------------------------------";

//
// Command line settings
//

std::string CMD_outFile   = "OUT.BIN";
std::string CMD_tmpDir    = "./out";
std::string CMD_segName   = "MAIN";
std::string CMD_segInfo   = "(unnamed)";
int         CMD_loAddress = 0xC000;
int         CMD_hiAddress = 0xCFFF;

std::list<std::string> CMD_dirList;

//
// Common helper functions
//

void bailOut()
{
	exit(-1);
}

void bailOut(const std::string &message)
{
	std::cout << std::endl << "FATAL: " << message << std::endl << std::endl;
	exit(-1);
}

void printUsage()
{
	std::cout << std::endl <<
	    "usage: build_segment [-o <out file>] [-d <out dir>] [-t <temp dir>]" << std::endl <<
	    "                     [-l <start/low address>] [-h <end/high address>]" << std::endl <<
        "                     [-s <segment name>] [-i <segment display info>] <input dir list>" <<
        std::endl << std::endl;
}

void printBannerLineTop()
{
	std::cout << std::endl << std::endl << std::endl << BANNER_LINE << std::endl;
}

void printBannerLineBottom()
{
	std::cout << BANNER_LINE << std::endl << std::endl;
}

void printBannerCollectAnalyse()
{
	printBannerLineTop();
	std::cout << "// Segment '" << CMD_segInfo << "' - collecting and analysing routines" << std::endl;
	printBannerLineBottom();
}

void printBannerBinCompile()
{
	printBannerLineTop();
	std::cout << "// Segment '" << CMD_segInfo << "' - binning and compiling the assembly" << std::endl;
    printBannerLineBottom();
}

//
// Class definitions
//

class SourceFile
{
public:
	SourceFile(const std::string &fileName, const std::string &dirName);

	std::string fileName;
	std::string dirName;

    bool floating;
	std::vector<char> content;

    std::string label;
    int  startAddr;    // for fixed (non-floating) routines only
    int  codeLength;   // for both fixed and floating routines

    int testAddrStart; // start address during test run
    int testAddrEnd;   // end address during test run
};

class BinningProblem
{
public:

	BinningProblem() {};
	BinningProblem(int loAddress, int hiAddress);	

	bool isSolved() const;

	void addToProblem(SourceFile *routine);
	void fillGap(int gapAddress, const std::list<SourceFile *> &routines);
	void performObviousSteps();
	void removeUselessGaps();
	void sortFloatingRoutinesBySize();

	std::map<int, SourceFile *> fixedRoutines;    // routines with location already fixed
	std::map<int, int>          gaps;             // gaps by address
	std::vector<SourceFile *>   floatingRoutines; // routines not allocated to any address yet; always keep them sorted!
};

class Solver
{
public:
	Solver(BinningProblem &problem) : problem(problem) {}

	void run();

	int selectGapToFill();
	void findPartialSolution(int gapSize, std::list<SourceFile *> &partialSolution);

private:

    BinningProblem &problem;
};

//
// Global variables
//

std::list<SourceFile> GLOBAL_sourceFiles;
std::list<SourceFile> GLOBAL_sourceFiles_noCode;
size_t                GLOBAL_maxFileNameLen    = 0;
size_t                GLOBAL_totalRoutinesSize = 0;
BinningProblem        GLOBAL_binningProblem;

//
// Top-level functions
//

void parseCommandLine(int argc, char **argv)
{
	int opt;

	// Retrieve command line options

    while ((opt = getopt(argc, argv, "o:d:t:l:h:s:i:")) != -1)
    {
        switch(opt)
        {
      	    case 'o': CMD_outFile   = optarg; break;
      	    case 't': CMD_tmpDir    = optarg; break;
      	    case 's': CMD_segName   = optarg; break;
      	    case 'i': CMD_segInfo   = optarg; break;
      	    case 'l': CMD_loAddress = strtol(optarg, nullptr ,16); break;
      	    case 'h': CMD_hiAddress = strtol(optarg, nullptr ,16); break;
            default: printUsage(); bailOut();
        }
    }

    // Retrieve directory list

    for (int idx = optind; idx < argc; idx++)
    {
        CMD_dirList.push_back(argv[idx]);
    }

    if (CMD_dirList.empty()) { printUsage(); bailOut("empty directory list"); }
}

void readSourceFiles()
{
	for (const auto &dirName : CMD_dirList)
	{
		DIR *dirHandle = opendir(dirName.c_str());
		if (!dirHandle) bailOut(std::string("unable to open directory '") + dirName + "'");

		struct dirent *dirEntry;
        while ((dirEntry = readdir(dirHandle)) != nullptr)
        {
        	const std::string fileName = dirEntry->d_name;

            // Filter-out files which are not assembler files, temporary, etc.

        	if (fileName.length() < 3)   continue;
            if (fileName.front() == '#') continue;
            if (fileName.front() == '~') continue;
            if (fileName.substr(fileName.length() - 2) != ".s") continue;

            // Add file to the global list

            GLOBAL_sourceFiles.push_back(SourceFile(fileName, dirName));
            GLOBAL_maxFileNameLen = std::max(GLOBAL_maxFileNameLen, fileName.length());
        }

        closedir(dirHandle);
	}

	if (GLOBAL_sourceFiles.empty()) bailOut("no source files found");

	// Sort the file list by name, to provide deterministic results

	auto compare = [](const SourceFile &a, const SourceFile &b) -> bool { return a.fileName.compare(b.fileName) < 0; }; 
	GLOBAL_sourceFiles.sort(compare);
}

void checkInputFileLabels()
{
	std::set<std::string> usedLabels;
	for (const auto &sourceFile : GLOBAL_sourceFiles)
	{
		if (usedLabels.count(sourceFile.label) != 0)
		{
			bailOut(std::string("input file '") + sourceFile.fileName + "' has a name too similar to another one");
		}

		usedLabels.insert(sourceFile.label);
	}
}

void calcRoutineSizes()
{
	const std::string nameBase = CMD_tmpDir + DIR_SEPARATOR + CMD_segName + "_sizetest";

    const std::string outFileName = nameBase + ".s";
    const std::string symFileName = nameBase + ".sym";

	// Remove old files

	unlink(outFileName.c_str());
	unlink(symFileName.c_str());

	// Write test file to determine routine sizes

    std::ofstream outFile(outFileName, std::fstream::out | std::fstream::trunc);
    if (!outFile.good()) bailOut(std::string("can't open temporary file '") + outFileName + "'");

    // Start at $100, so that no local data gets accesses using ZP addressing modes
    // during this pass,  which would otherwise upset things later

    outFile << std::endl << ".segment " << CMD_segName << " [start=$100, min=$100, max=$FFFF]" << std::endl;

    for (const auto &sourceFile : GLOBAL_sourceFiles)
    {
    	outFile << std::endl << std::endl << std::endl << std::endl;
    	outFile << "// Source file: " << sourceFile.fileName << std::endl << std::endl;
    	outFile << LAB_OUT_START << sourceFile.label << ":" << std::endl << std::endl;

    	outFile << std::string(sourceFile.content.begin(), sourceFile.content.end());

        outFile << std::endl << std::endl;
    	outFile << LAB_OUT_END << sourceFile.label << ":" << std::endl;
    }

    if (!outFile.good()) bailOut(std::string("error writing temporary file '") + outFileName + "'");
    outFile.close();

    // All written - now launch the assembler

    const std::string cmd = ASM_CMD + outFileName + " -symbolfile -o /dev/null";
    std::cout << std::flush;
    if (0 != system(cmd.c_str()))
    {
    	bailOut("assembler running failed");
    }

    // Read addresses

	std::ifstream symFile;
    symFile.open(symFileName);
    if (!symFile.good()) bailOut(std::string("unable to open results file '") + symFileName + "'");

	std::string line;
	while (std::getline(symFile, line))
	{
		bool startLabel;

		if (0 == line.compare(0, LAB_IN_START.size(), LAB_IN_START))
		{
			line.erase(0, LAB_IN_START.length());
			startLabel = true;
		}
		else if (0 == line.compare(0, LAB_IN_END.size(), LAB_IN_END))
		{
			line.erase(0, LAB_IN_END.length());
			startLabel = false;
		}
		else continue;

		// Decode address, write it into the object

		auto eqPos = line.rfind('=');

		auto address         = strtol(line.substr(eqPos + 2).c_str(), nullptr ,16); 
		std::string refLabel = line.substr(0, eqPos);

		for (auto &sourceFile : GLOBAL_sourceFiles)
		{
			if (refLabel.compare(sourceFile.label) != 0) continue;
			if (startLabel)
			{
				sourceFile.testAddrStart = address;
			}
			else
			{
				sourceFile.testAddrEnd = address;				
			}
		}
	}

    symFile.close();

    // Calculate size of each and every routine

    for (auto &sourceFile : GLOBAL_sourceFiles)
	{
		if (sourceFile.testAddrStart <= 0 || sourceFile.testAddrEnd <= 0 ||
			sourceFile.testAddrStart > sourceFile.testAddrEnd)
		{
			bailOut(std::string("unable to determine code length in '") + sourceFile.fileName + "'");
		}

		sourceFile.codeLength = sourceFile.testAddrEnd - sourceFile.testAddrStart;
		GLOBAL_totalRoutinesSize += sourceFile.codeLength;
	}

	// Check whether total code size is sane

	if (0 == GLOBAL_totalRoutinesSize) bailOut("total code size is 0");
	if (signed(GLOBAL_totalRoutinesSize) > CMD_hiAddress - CMD_loAddress)
	{
		bailOut(std::string("total code size is ") + std::to_string(GLOBAL_totalRoutinesSize) + ", too much for this segment!");
	}

	// Sort the file list by code size, starting from the smallest

	auto compare = [](const SourceFile &a, const SourceFile &b) -> bool { return a.codeLength < b.codeLength; }; 
	GLOBAL_sourceFiles.sort(compare);

	// Move zero-sized element to a separate list

	while (GLOBAL_sourceFiles.front().codeLength == 0)
	{
		GLOBAL_sourceFiles_noCode.push_back(GLOBAL_sourceFiles.front());
		GLOBAL_sourceFiles.pop_front();
	}
}

void prepareBinningProblem()
{
	// Prepare the log file

	const std::string logFileName = CMD_tmpDir + DIR_SEPARATOR + CMD_segName + "_binproblem.log";
	unlink(logFileName.c_str());
	std::ofstream logFile(logFileName, std::fstream::out | std::fstream::trunc);
	std::string logLine;

	// Print out code length information

	for (const auto &sourceFile : GLOBAL_sourceFiles)
	{
		std::string spacing;
		spacing.resize(GLOBAL_maxFileNameLen + 4 - sourceFile.fileName.length(), ' ');
		std::string floating = sourceFile.floating ? "(floating)    " : "              ";
		logLine = "file:    " + floating + sourceFile.fileName + spacing + "size: " + std::to_string(sourceFile.codeLength);

		logFile   << logLine << std::endl;
		std::cout << logLine << std::endl;
	}

    // Create the binning problem

    GLOBAL_binningProblem = BinningProblem(CMD_loAddress, CMD_hiAddress);

    for (auto &sourceFile : GLOBAL_sourceFiles)
    {
    	GLOBAL_binningProblem.addToProblem(&sourceFile);
    }

    // Print out some more statistics

	logLine = "free space (after floating routines are placed):    " +
	          std::to_string(CMD_hiAddress - CMD_loAddress - GLOBAL_totalRoutinesSize) ;
	logFile   << std::endl << logLine << std::endl;
	std::cout << std::endl << logLine << std::endl;
    logLine = "number of floating routines:                        " +
              std::to_string(GLOBAL_binningProblem.floatingRoutines.size());
	logFile   << logLine << std::endl;
	std::cout << logLine << std::endl;    
    logLine = "number of gaps for the floating routines:           " +
              std::to_string(GLOBAL_binningProblem.gaps.size());
	logFile   << logLine << std::endl << std::endl;
	std::cout << logLine << std::endl << std::endl;

    // Print out the available gaps

	logLine = "gap address: $";
    for (auto& gap : GLOBAL_binningProblem.gaps)
    {
    	std::string gapSize = std::string("    size: ") + std::to_string(gap.second);
    	logFile   << logLine << std::uppercase << std::hex << gap.first << gapSize << std::endl;
    	std::cout << logLine << std::uppercase << std::hex << gap.first << gapSize << std::endl;
    }

    std::cout << std::endl;

    // Close the log file

    if (!logFile.good()) bailOut(std::string("error writing log file '") + logFileName + "'");
	logFile.close();
}

void solveBinningProblem()
{
	std::cout << "trying to solve the routine binning problem" << std::endl << std::endl;

	// Do some routine actions on the binning problem object

	Solver solver(GLOBAL_binningProblem);
	solver.run();

	if (!GLOBAL_binningProblem.isSolved())
	{
		bailOut("unable to solve the routine binning problem");
	}

	std::cout << std::endl;
}

void compileSegment()
{
	// First combine everything into one assembler file

	const std::string outFileName = CMD_tmpDir + DIR_SEPARATOR + CMD_segName + "_combined.s";
	unlink(outFileName.c_str());
	std::ofstream outFile(outFileName, std::fstream::out | std::fstream::trunc);
	if (!outFile.good()) bailOut(std::string("can't open temporary file '") + outFileName + "'");

	// Write the header

	outFile << std::endl << ".segment " << CMD_segName <<
	            " [start=$" << std::hex << CMD_loAddress <<
                ", min=$" << std::hex << CMD_loAddress <<
                ", max=$" << std::hex << CMD_hiAddress <<
                ", outBin=\"" << CMD_outFile << "\", fill]" <<
                std::endl;

	// Write files which only contain definitions (no routines)

    for (const auto &sourceFile : GLOBAL_sourceFiles_noCode)
    {
    	outFile << std::endl << std::endl << std::endl << std::endl;
    	outFile << "// Source file: " << sourceFile.fileName << std::endl << std::endl;
    	outFile << std::string(sourceFile.content.begin(), sourceFile.content.end());
    	outFile << std::endl;
    }

    // Write remaining files, these should be placed under their proper locations

    for (const auto &routine : GLOBAL_binningProblem.fixedRoutines)
    {
    	outFile << std::endl << std::endl << std::endl << std::endl;
    	outFile << "// Source file: " << routine.second->fileName << std::endl << std::endl;
    	outFile << "\t* = $" << std::hex << routine.first << std::endl << std::endl;
    	outFile << std::string(routine.second->content.begin(), routine.second->content.end());
    	outFile << std::endl;
    }

    if (!outFile.good()) bailOut(std::string("error writing temporary file '") + outFileName + "'");
    outFile.close();

    // All written - now launch the assembler

    const std::string cmd = ASM_CMD + outFileName + " -symbolfile -o " + CMD_outFile;
    std::cout << std::flush;
    if (0 != system(cmd.c_str()))
    {
    	bailOut("assembler running failed");
    }
}

//
// Main function
//

int main(int argc, char **argv)
{
	parseCommandLine(argc, argv);

	printBannerCollectAnalyse();

	readSourceFiles();
	checkInputFileLabels();
	calcRoutineSizes();

	printBannerBinCompile();

	prepareBinningProblem();
	solveBinningProblem();
	compileSegment();

	return 0;
}

//
// Class 'SourceFile'
//

SourceFile::SourceFile(const std::string &fileName, const std::string &dirName) :
    fileName(fileName),
    dirName(dirName),
    floating(false),
    startAddr(-1),
    codeLength(-1),
    testAddrStart(-1),
    testAddrEnd(-1)
{
	const std::string fileNameWithPath = dirName + DIR_SEPARATOR + fileName;
	std::cout << "reading file: " << fileNameWithPath << std::endl;

	// Open the file

	std::ifstream inFile;
    inFile.open(fileNameWithPath);
    if (!inFile.good()) bailOut("unable to open file");

    // Read the content

    inFile.seekg(0, inFile.end);
    auto fileLength = inFile.tellg();
    if (fileLength == 0) bailOut("file is empty");
    content.resize(fileLength);

    inFile.seekg(0);
    inFile.read(&content[0], fileLength);
    if (!inFile.good()) bailOut("error reading file content");

    inFile.close();

    // Determine if the file content is floating or fixed position one,
    // retrieve start address

    auto isHexDigit = [](char digit) -> bool
    {
    	return (digit >= '0' && digit <= '9') ||
    	       (digit >= 'a' && digit <= 'f') ||
    	       (digit >= 'A' && digit <= 'F');;
    };

    floating = !(fileName.length() >= 6 && fileName[4] == '.' &&
    	         isHexDigit(fileName[0]) &&
    	         isHexDigit(fileName[1]) &&
    	         isHexDigit(fileName[2]) &&
    	         isHexDigit(fileName[3]));

    if (!floating)
    {
    	startAddr = strtol(fileName.substr(0, 4).c_str(), nullptr ,16); 
    }

    // Generate KickAss compatible label from the file name

    label = fileName.substr(0, fileName.length() - 2);

    std::replace(label.begin(), label.end(), '.', '_');
    std::replace(label.begin(), label.end(), ',', '_');
}

//
// Class 'BinningProblem'
//

BinningProblem::BinningProblem(int loAddress, int hiAddress)
{
	if (hiAddress < loAddress || hiAddress < 0 || loAddress < 0 || hiAddress > 0xFFFF || loAddress > 0xFFFF)
	{
		bailOut("invalid lo/hi address");
	}

	// Create one large gap (initial) to represent the assembly

	gaps[loAddress] = hiAddress - loAddress + 1;
}

bool BinningProblem::isSolved() const
{
	return floatingRoutines.empty();
}

void BinningProblem::addToProblem(SourceFile *routine)
{
	if (routine->floating)
	{
		floatingRoutines.push_back(routine);
	}
	else
	{
		// For the fixed-address routines, we have to find the matching place

		bool gapFound = false;
		for (auto &gap: gaps)
		{
			if (gap.first > routine->startAddr || gap.first + gap.second - 1 < routine->startAddr)
			{
				continue; // not a suitable gap
			}

			// We have to put the routine into the current gap

			if (gap.first + gap.second < routine->startAddr + routine->codeLength)
			{
				bailOut(std::string("fixed address file '") + routine->fileName + "' won't fit in the available gap");
			}

			gapFound = true;

			// Put the routine into the gap, possibly removing it or splitting into two

			fixedRoutines[routine->startAddr] = routine;

			// Calculate possible new gap after the routine

			int newGapSize  = (gap.first + gap.second) - (routine->startAddr + routine->codeLength);
			int newGapStart = (newGapSize <= 0) ? -1 : routine->startAddr + routine->codeLength;

			// Remove or shrink the current gap

			if (gap.first == routine->startAddr)
			{
				gaps.erase(gap.first);
			}
			else
			{
				gap.second = routine->startAddr - gap.first;
			}

			// Add a new gap

			if (newGapStart > 0) gaps[newGapStart] = newGapSize;

            break; // iterator is not valid enymore, we have to stop the llop
		}

		if (!gapFound)
		{
			bailOut(std::string("start address of fixed address file '") + routine->fileName + "' already occupied");
		}
	}
}

void BinningProblem::fillGap(int gapAddress, const std::list<SourceFile *> &routines)
{
	int offset = 0;
	std::string spacing;

	for (auto &routine : routines)
	{
		int targetAddr = gapAddress + offset;

		spacing.resize(GLOBAL_maxFileNameLen + 4 - routine->fileName.length(), ' ');
		std::cout << "    $" << std::hex << targetAddr << std::dec << ": " <<
                     routine->fileName << spacing << "size: " << routine->codeLength << std::endl;

        fixedRoutines[targetAddr] = routine;
        offset += routine->codeLength;

        if (offset > gaps[gapAddress]) bailOut(std::string("internal error line ") + std::to_string(__LINE__));

        floatingRoutines.erase(std::remove(floatingRoutines.begin(), floatingRoutines.end(), routine), floatingRoutines.end());
	}

	// Get rid of the gap, it's useless now

	if (offset == gaps[gapAddress])
    {
    	std::cout << "filled to the last byte" << std::endl;
    }
    else if (!isSolved())
    {
    	std::cout << "filled in - dropped bytes: " << gaps[gapAddress] << std::endl;
    }
    else
    {
    	std::cout << "out of routines" << std::endl;
    }

	gaps.erase(gapAddress);
}

void BinningProblem::performObviousSteps()
{
	// Get the size of the biggest routine; if there is just one gap which
	// can handle it - put the roputine exactly there

	std::string spacing;

	bool repeat = true;
	while (repeat && !floatingRoutines.empty() && !gaps.empty())
	{
		repeat = false;

		int routineSize = floatingRoutines.back()->codeLength;

		int gapAddress;
		int matchingGaps = 0;
		for (auto &gap : gaps)
		{
			if (gap.second >= routineSize)
			{
				matchingGaps++;
				gapAddress = gap.first;
			}
		}

		if (matchingGaps == 1)
		{
			// Routine can only be placed in this particular gap - do so

			gaps[gapAddress] -= routineSize;
			int targetAddr = gapAddress + gaps[gapAddress];
			fixedRoutines[targetAddr] = floatingRoutines.back();

			std::cout << "forced reducing gap $" << std::hex << gapAddress << std::dec << " to size " << 
			             gaps[gapAddress] << std::endl;

			spacing.resize(GLOBAL_maxFileNameLen + 4 - floatingRoutines.back()->fileName.length(), ' ');
			std::cout << "    $" << std::hex << targetAddr << std::dec << ": " <<
	                     floatingRoutines.back()->fileName << spacing << "size: " <<
	                     floatingRoutines.back()->codeLength << std::endl;

			floatingRoutines.pop_back();
			if (gaps[gapAddress] == 0) gaps.erase(gapAddress);

			repeat = true;
		}
	}
}

void BinningProblem::removeUselessGaps()
{
	// Get the size of the smallest floating routine,
	// remove all the gaps which are smaller in size

	int minUsefulSize = floatingRoutines[0]->codeLength;

	bool repeat = true;
	while (repeat)
	{
		repeat = false;
		for (auto &gap : gaps)
		{
			if (gap.second < minUsefulSize)
			{
				std::cout << "dropping gap: $" << std::hex << gap.first << std::dec << " (size: " << gap.second << ")" << std::endl;
				gaps.erase(gap.first);
				repeat = true;
				break;
			}
		}
	}
}

void BinningProblem::sortFloatingRoutinesBySize()
{
	// Sort the unallocated routines by size, starting from the smallest one

    auto compare = [](const SourceFile *a, const SourceFile *b) -> bool { return a->codeLength < b->codeLength; }; 
	std::sort (floatingRoutines.begin(), floatingRoutines.end(), compare);
}

//
// Class 'Solver'
//

void Solver::run()
{
	problem.sortFloatingRoutinesBySize(); // just to be extra sure

	while (!problem.gaps.empty() && !problem.floatingRoutines.empty())
	{
		problem.performObviousSteps();
		problem.removeUselessGaps();

		if (problem.gaps.empty() || problem.floatingRoutines.empty()) break;

		int gapAddr = selectGapToFill();

		std::cout << "selected gap: " << "$" << std::hex << gapAddr << std::dec << " (size: " << problem.gaps[gapAddr] << ")" << std::endl;

		std::list<SourceFile *> partialSolution;
		findPartialSolution(problem.gaps[gapAddr], partialSolution);
		problem.fillGap(gapAddr, partialSolution);
	}

	if (problem.isSolved())
	{
		std::cout << std::endl << "all the routines sucessfully placed" << std::endl << std::endl;
	}
}

int Solver::selectGapToFill()
{
	// Find the smallest gap to fill-in

	int gapAddr = -1;
	int gapSize = -1;

	for (auto &gap : problem.gaps)
	{
		if (gapSize < 0 || gap.second < gapSize)
		{
			gapAddr = gap.first;
			gapSize = gap.second;
		}
	}

	return gapAddr;
}

int KS(const std::vector<SourceFile *> &routines,
	std::vector<std::vector<int>> &cacheV,             // value cache
	std::vector<std::vector<std::list<bool>>> &cacheS, // partial solution cache
	int n, int C,
	std::list<bool> &solution) // sequence of decisions for each routine, has to be empty when calling
{
	// This routine solves a knapsack problem using a dynamic programming

	// XXX Possible efficiency improvement for the future: try to consider more than one gap at once,
	//     this will need a serious rework of the dynamic cache handling and will probably hurt the performance

	if (n == 0)
	{
		// No more routines to take the decision...
		return 0;
	}

	if (C == 0)
	{
		// We have no capacity left - decisions for all the routines left should be not to take them
		solution.resize(n, false);
		return 0;
	}

	auto &cachedV = cacheV[n - 1][C - 1];
	auto &cachedS = cacheS[n - 1][C - 1];
	if (cachedV >= 0)
	{
		// We already hold the solution in out cache - return it
		solution = cachedS;
		return cachedV;
	}

	auto &codeSize = routines[n - 1]->codeLength;
	if (codeSize > C)
	{
		// We can't take this routine, it's too large

		cachedV = KS(routines, cacheV, cacheS, n - 1, C, solution);
		solution.push_back(false);
		cachedS = solution;

		return cachedV;	
	}

	// Note: it it seems equally good to take this routine, or not - take it!
	// Evaluation starts from the largest routines, and we should prefer to leave
	// a larger number of smaller routines - as this gives more possibilities
	// while optimizing the usage of further gaps

	std::list<bool> solution1;
	int val1 = KS(routines, cacheV, cacheS, n - 1, C - codeSize, solution1) + codeSize; // take

	if (val1 == C)
	{
		// By taking this routine we are filling all the space - contrary to the classic
		// knapsack problem, this is an optimal solution for us, don't waste time calculating
		// the others

		cachedV = val1;
		solution = solution1;
		solution.push_back(true);
		cachedS = solution;
		return cachedV;
	}

	std::list<bool> solution2;
	int val2 = KS(routines, cacheV, cacheS, n - 1, C, solution2); // don't take

	if (val1 >= val2)
	{
		// It's better to take this particular routine

		cachedV = val1;
		solution = solution1;
		solution.push_back(true);
	}
	else
	{
		// It's better not to take this particular routine

		cachedV = val2;
		solution = solution2;
		solution.push_back(false);
	}

	cachedS = solution;
	return cachedV;
};

void Solver::findPartialSolution(int gapSize, std::list<SourceFile *> &partialSolution)
{
	// First filter out available routines, take only the ones not larger than our gap

    std::vector<SourceFile *> routines;
	routines.clear();
	for (auto &routine : problem.floatingRoutines)
	{
		if (routine->codeLength <= gapSize)
		{
			routines.push_back(routine);
		}
		else break; // 'floatingRoutines' should be kept sorted
	}

	// Prepare value cache

	std::vector<std::vector<int>> cacheV;
	cacheV.resize(routines.size());
	for (auto &cacheRow : cacheV) cacheRow.resize(gapSize, -1);

	// Prepare partial solution cache
	std::vector<std::vector<std::list<bool>>> cacheS;
	cacheS.resize(routines.size());
	for (auto &cacheRow : cacheS) cacheRow.resize(gapSize);

	// Calculate solution

	std::list<bool> solution;
	KS(routines, cacheV, cacheS, routines.size(), gapSize, solution);

	if (routines.size() != solution.size()) bailOut(std::string("internal error line ") + std::to_string(__LINE__));

	// Return the solution upstream

	while (!routines.empty())
	{
		if (solution.back() != false)
		{
			partialSolution.push_front(routines.back());
		}

		routines.pop_back();
		solution.pop_back();
	}

	return;
}
