#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <zlib.h>
#include <cstring>
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>
#include "headers.h"
using namespace std;

int main(int argc, char *argv[])
{
    // Flush after every cout / cerr
    cout << unitbuf;
    cerr << unitbuf;

    // You can use print statements as follows for debugging, they'll be visible when running tests.
    // cout << "Logs from your program will appear here!\n";

    // Uncomment this block to pass the first stage
    //
    if (argc < 2) {
        cerr << "No command provided.\n";
        return EXIT_FAILURE;
    }
    
    string command = argv[1];
    
    if (command == "init") {
        try {
            filesystem::create_directory(".git");
            filesystem::create_directory(".git/objects");
            filesystem::create_directory(".git/refs");
    
            ofstream headFile(".git/HEAD");
            if (headFile.is_open()) {
                headFile << "ref: refs/heads/main\n";
                headFile.close();
            } else {
                cerr << "Failed to create .git/HEAD file.\n";
                return EXIT_FAILURE;
            }
    
            cout << "Initialized git directory\n";
        } catch (const filesystem::filesystem_error& e) {
            cerr << e.what() << '\n';
            return EXIT_FAILURE;
        }
    } else if(command == "cat-file"){
        try{
            if (argc < 4 &&(string(argv[2]) != "-p" || string(argv[2]) != "-t" || string(argv[2]) != "-s")) {
            cerr << "Missing parameter: -p <hash>\n";
            return EXIT_FAILURE;
            }

            string hash = argv[3];
            if (hash.size() != 40) {
                cerr << "Invalid Git blob hash length.\n";
                return 1;
            }
            string commandFlag = argv[2];
            string filePath = getFilePathFromSHA(hash);
            decompressGitObjectChunkwise(filePath,hash,commandFlag);
        } catch(const exception& ex){
            cerr << "Error: " << ex.what() << endl;
            return 1;
        }
    } else if(command == "hash-object"){
        try
        {
            if (argc < 4 || string(argv[2]) != "-w") {
            cerr << "Missing parameter: -w\n";
            return EXIT_FAILURE;
            }
            string filename = argv[3];
            string content = readFile(filename);
            string blobHeader = "blob " +to_string(content.size()) + '\0';
            string blobContent = blobHeader + content;
            string sha1 = calculateSHA1(blobContent);
            string compressedContent = compressContent(blobContent);
            
            storeCompressedFile(sha1, compressedContent);
            cout << sha1 << endl;
        }
        catch(const exception& e)
        {
            cerr << e.what() << '\n';
            return 1;
        }
        
    } else if(command == "ls-tree"){
        try{
           if (argc < 3) {
                cerr << "Missing parameters\n";
                return EXIT_FAILURE;
            } 
            if (argc == 4 && string(argv[2]) != "--name-only") {
                cerr << "Missing parameter: --name-only <hash>\n";
                return EXIT_FAILURE;
            }

            if(argc == 3){
                string hash = argv[2];
                if (hash.size() != 40) {
                    cerr << "Invalid Git blob hash length.\n";
                    return 1;
                }
                read_and_decompress_git_object(hash,false);
            } else {
                string hash = argv[3];
                if (hash.size() != 40) {
                    cerr << "Invalid Git blob hash length.\n";
                    return 1;
                }
                read_and_decompress_git_object(hash,true);
                
            }
        }
        catch(const exception& e)
        {
            cerr << e.what() << '\n';
            return 1;
        }
    } else if(command == "write-tree"){
        try{
            if (argc < 2)
            {
                cerr << "Invalid command.\n";
                return EXIT_FAILURE;
            }
            filesystem::path cwd = filesystem::current_path();


            return writeTree(cwd);
        }
        catch(const exception& e)
        {
            cerr << e.what() << '\n';
            return 1;
        }
    } else if(command == "commit-tree"){
        try{
            if (argc != 7 && argc != 5)
            {
                cerr << "Invalid command.\n";
                return EXIT_FAILURE;
            }

        string sha = argv[2];
        string flag1 = "";
        string sha_parent = "";
        string flag2 = "";
        string message = "";
        if(argc == 7){
            flag1 =  argv[3];
            sha_parent = argv[4];
            flag2 =  argv[5];
            message = argv[6];
            if(flag2 != "-m"){
                cerr << "Missing parameter: -m\n";
                return EXIT_FAILURE;
            }
        }else if(argc == 5){
            flag2 =  argv[3];
            message = argv[4];
            if (flag2 != "-m") {
                cerr << "Missing or incorrect parameter: expected '-m'\n";
                return EXIT_FAILURE;
            }
        }
        commitTree(flag1, sha_parent, flag2, message, sha);
        return 0;
        }
        catch(const exception& e){
            cerr << e.what() << '\n';
            return 1;
        }
    } else if(argc > 1 && std::string(argv[1]) == "add"){
        try {
            if (argc < 3) {
            cerr << "Missing parameters for add command.\n";
            return EXIT_FAILURE;
            }

            std::vector<std::string> paths;
            for (int i = 2; i < argc; ++i) {
                paths.push_back(argv[i]);
            }
            addFiles(paths);
        }
        catch (const exception& e) {
            cerr << e.what() << '\n';
            return 1;
        }
    } else if(argc > 1 && std::string(argv[1]) == "commit"){
        try {
            std::string message;
            if (argc == 2) {
            message = "Default commit message";
            } else if (argc == 4 && std::string(argv[2]) == "-m") {
                message = argv[3];
            } else {
            cerr << "Invalid parameters for commit command.\n";
            return EXIT_FAILURE;
            }

            std::string indexPath = ".git/index";
            commit(indexPath, message);
        }
        catch (const exception& e) {
            cerr << e.what() << '\n';
            return 1;
        }
    } else if(argc > 1 && std::string(argv[1]) == "log"){
        try {
           printLogs();
        }
        catch (const exception& e) {
            cerr << e.what() << '\n';
            return 1;
        }
    } else if (command == "checkout") {
        if (argc != 3) {
            std::cerr << "Usage: checkout <sha>\n";
            return EXIT_FAILURE;
        }

        std::string sha = argv[2];
        try {
            extractCommit(sha);
        } catch (const std::exception& e) {
            std::cerr << "Error during checkout: " << e.what() << '\n';
            return EXIT_FAILURE;
        }
    } 
    else{
        cerr << "Unknown command " << command << '\n';
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}