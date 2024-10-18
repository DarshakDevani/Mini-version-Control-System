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
#include <ctime>
#include <chrono>
#include <map>
#include "headers.h"
using namespace std;
namespace fs = std::filesystem;

const int CHUNK_SIZE = 16384;  // Input and output buffer size


void decompressGitObjectChunkwise(const string& filepath, string hash, string commandFlag) {
    ifstream file(filepath, ios::binary);
    if (!file.is_open()) {
        throw runtime_error("Unable to open file: " + filepath);
    }

    // Initialize zlib stream
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (inflateInit(&zs) != Z_OK) {
        throw runtime_error("Failed to initialize zlib for decompression");
    }

    unsigned char inputBuffer[CHUNK_SIZE];  // Buffer to read compressed data
    unsigned char outputBuffer[CHUNK_SIZE]; // Buffer to hold decompressed data

    int ret;
    bool foundNullChar = false;  // Persistent flag across chunks
    bool flag = false;           // Indicates if we've encountered the first '\0'
    vector<string> header;       // Vector to store parsed header fields
    string msg = "";             // Temporary string to accumulate header fields

    do {
        // Read a chunk of compressed data into inputBuffer
        file.read(reinterpret_cast<char*>(inputBuffer), CHUNK_SIZE);
        streamsize bytesRead = file.gcount();  // Get number of bytes read
        if (bytesRead == 0) break;  // EOF reached

        zs.avail_in = bytesRead;      // Input bytes available for decompression
        zs.next_in = inputBuffer;     // Pointer to input buffer

        // Decompress chunk
        do {
            zs.avail_out = CHUNK_SIZE;     // Space available in output buffer
            zs.next_out = outputBuffer;    // Pointer to output buffer

            ret = inflate(&zs, Z_NO_FLUSH);
            if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
                inflateEnd(&zs);
                throw runtime_error("Decompression error occurred");
            }

            int bytesDecompressed = CHUNK_SIZE - zs.avail_out;  // Bytes decompressed

            // Process the decompressed data
            for (int i = 0; i < bytesDecompressed; ++i) {
                if (flag) {
                    // We've encountered the '\0', process the content based on flags
                    if (commandFlag == "-s") {
                        // Print the 3rd part of the header (size information)
                        cout << header[1] << endl;
                        return;
                    } else if (commandFlag == "-t") {
                        // Print the 1st part of the header (type information)
                        cout << header[0] << endl;
                        return;
                    } else if (commandFlag == "-p") {
                        // Print the object content (for trees, recursively decompress)
                        if (header[0] == "tree") {
                            read_and_decompress_git_object(hash, false);
                            return;
                        } else {
                            // Output the decompressed content
                            cout.write(reinterpret_cast<char*>(&outputBuffer[i]), bytesDecompressed - i);
                        }
                        return;
                    }
                } else if (outputBuffer[i] == '\0') {
                    // First occurrence of '\0', start processing content
                    if (!msg.empty()) {
                        header.push_back(msg);  // Push the final header field
                    }
                    flag = true;
                } else {
                    // Accumulate header before '\0'
                    if (outputBuffer[i] == ' ') {
                        header.push_back(msg);
                        msg.clear();  // Reset msg after each space (for next field)
                    } else {
                        msg += outputBuffer[i];  // Accumulate characters
                    }
                }
            }

        } while (zs.avail_out == 0);  // Continue if output buffer was filled

    } while (ret != Z_STREAM_END);  // Continue until end of stream is reached

    inflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        throw runtime_error("Decompression incomplete. Stream ended unexpectedly.");
    }
}

string getFilePathFromSHA(const string& sha) {
    if (sha.length() != 40) {
        throw invalid_argument("Invalid SHA format. SHA must be 40 characters long.");
    }
    
    string directory = sha.substr(0, 2);       // First 2 characters
    string filename = sha.substr(2, 38);       // Next 38 characters
    
    return filesystem::path(".git") / "objects" / directory / filename;
}

string readFile(const string &filename) {
    ifstream file(filename, ios::binary);
    if (!file) {
        throw runtime_error("Failed to open file: " + filename);
    }
    ostringstream buffer;
    buffer << file.rdbuf();
    file.close();
    return buffer.str();
}

string calculateSHA1(const string &input) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char *>(input.c_str()), input.size(), hash);

    ostringstream result;
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        result << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    return result.str();
}


string compressContent(const string &content) {
    uLongf compressedSize = compressBound(content.size());
    string compressedData(compressedSize, '\0');

    int result = compress(reinterpret_cast<Bytef *>(&compressedData[0]), &compressedSize,
                          reinterpret_cast<const Bytef *>(content.c_str()), content.size());

    if (result != Z_OK) {
        throw runtime_error("Failed to compress content.");
    }

    compressedData.resize(compressedSize); // Resize to the actual compressed size
    return compressedData;
}

void storeCompressedFile(const string &sha1, const string &compressedContent) {
    string directory = ".git/objects/" + sha1.substr(0, 2);
    string filename = sha1.substr(2);

    // Create the directory if it doesn't exist
    mkdir(directory.c_str(), 0777);

    // Create the file
    string filepath = directory + "/" + filename;
    ofstream outfile(filepath, ios::binary);
    outfile.write(compressedContent.c_str(), compressedContent.size());
    outfile.close();
}

string to_hex_string(const unsigned char *data, size_t length) {
    ostringstream result;
    for (size_t i = 0; i < length; ++i) {
        result << hex << setw(2) << setfill('0') << (int)data[i];
    }
    return result.str();
}

// Function to read a compressed Git object from .git/objects and decompress it using Zlib
bool read_and_decompress_git_object(const string &sha1_hash,bool flag, const string &git_dir) {
    // Construct the path to the object file based on the SHA-1 hash
    string dir_name = sha1_hash.substr(0, 2);  // First 2 characters are the directory
    string file_name = sha1_hash.substr(2);    // Remaining part is the file name
    string object_path = git_dir + "/objects/" + dir_name + "/" + file_name;

    // Open the object file in binary mode
    ifstream file(object_path, ios::binary);
    if (!file.is_open()) {
        throw runtime_error("Error: Unable to open Git object file.");
    }

    // Read the compressed file data
    vector<unsigned char> compressed_data((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    file.close();

    // Prepare to decompress the data using Zlib
    z_stream infstream;
    infstream.zalloc = Z_NULL;
    infstream.zfree = Z_NULL;
    infstream.opaque = Z_NULL;
    infstream.avail_in = compressed_data.size(); // Size of input data
    infstream.next_in = compressed_data.data();  // Input data

    // Decompressed output buffer (a large buffer to hold the decompressed data)
    vector<unsigned char> decompressed_data(1024 * 1024); // 1 MB buffer

    infstream.avail_out = decompressed_data.size();
    infstream.next_out = decompressed_data.data();

    // Initialize the Zlib decompression
    if (inflateInit(&infstream) != Z_OK) {
        throw runtime_error("Error: Failed to initialize zlib.");
    }

    // Decompress the data
    int result = inflate(&infstream, Z_FINISH);
    if (result != Z_STREAM_END) {
        inflateEnd(&infstream);
        throw runtime_error("Error: Failed to decompress Git object.");
    }

    // Clean up Zlib
    inflateEnd(&infstream);

    // Resize decompressed_data to the actual size of decompressed content
    decompressed_data.resize(infstream.total_out);

    string header;
    for (size_t i = 0; i < decompressed_data.size() && decompressed_data[i] != '\0'; i++) {
        header += static_cast<char>(decompressed_data[i]);
    }

    // The header contains the object type followed by a space and the size
    if (header.find("blob") == 0) {
        // If the object is a blob, call decompressGitObjectChunkwise
        decompressGitObjectChunkwise(object_path, sha1_hash, "-p");
    } else if (header.find("commit") == 0) {
        // If the object is a commit, call decompressGitObjectChunkwise
        decompressGitObjectChunkwise(object_path, sha1_hash, "-p");
    } else if (header.find("tree") == 0) {
        // If the object is a tree, call parse_tree_object
        parse_tree_object(decompressed_data, flag);
    } else {
        throw runtime_error("Error: Unknown Git object type.");
    }

    return true;
}

// Function to parse a decompressed Git tree object
void parse_tree_object(vector<unsigned char> &data, bool option) {
    size_t i = 0;

    // Skip the header ("tree <size>")
    
    while (data[i] != 0) {
        i++;
    }
    i++; // Skip the null byte after the header

    // Parse the tree entries

    while (i < data.size()) {
        // Extract the file mode (up to the first space)
        string mode;
        while (data[i] != ' ') {
            mode += data[i];
            i++;
        }
        if(mode[0] == '4') mode = '0' + mode;
        i++; // Skip the space

        // Extract the filename (up to the null byte)
        string filename;
        while (data[i] != '\0') {
            filename += data[i];
            i++;
        }
        i++; // Skip the null byte

        // Extract the SHA-1 hash (next 20 bytes)
        string sha1 = to_hex_string(&data[i], 20);
        i += 20;

        if(option){
            cout<<filename<<endl;
            continue;
        }
        // Print the extracted entry
        string objectType;
        if(mode.substr(0,3) == "040"){
            objectType = "tree";
        }else if(mode.substr(0,3) == "100" || mode.substr(0,3) == "120"){
            objectType = "blob";
        }else if(mode.substr(0,3) == "160"){
            objectType = "commit";
        }
        cout << mode << " " <<objectType<<" "<< sha1 << "   " << filename << endl;
    }
}

string HexadecimalSha(const string& sha)
{
    ostringstream sha_hexadecimal;
    for (unsigned char char_ : sha)
    {
        sha_hexadecimal << hex << setw(2) << setfill('0') << (int)(char_);
    }
    return sha_hexadecimal.str();
}

string CreateBlobString(const string& filename)
{
    ifstream file(filename);
    if (!file.is_open())
    {
        cerr << filename << " not found.\n";
        throw runtime_error("");
    }
    vector<char> raw_file =
        vector<char>(istreambuf_iterator<char>(file), istreambuf_iterator<char>());
    string blob =
        "blob " + to_string(raw_file.size()) + '\0' + string(raw_file.begin(), raw_file.end());
    file.close();
    return blob;
}

string ComputeShaHash(const string& data)
{
    string hash(SHA_DIGEST_LENGTH, '\0');  // Create a string with SHA_DIGEST_LENGTH (20) null characters
    SHA1(reinterpret_cast<const unsigned char*>(data.c_str()), data.size(), reinterpret_cast<unsigned char*>(&hash[0]));
    return hash;  // Return the 20-byte binary hash
}

string _WriteTree(const filesystem::path& path)
{
    if (filesystem::is_empty(path))
    {
        return string("");
    }
    vector<filesystem::directory_entry> entries;
    for (const auto& entry : filesystem::directory_iterator(path))
    {
        if (entry.path().filename() == ".git" || entry.path().filename() == "build" || entry.path().filename() == "vcpkg" || entry.path().filename() == ".git" || entry.path().filename() == "CMakeLists.txt" || entry.path().filename() == ".DS_Store")
        {
            continue;
        }
        entries.push_back(entry);
    }
    sort(entries.begin(), entries.end(), [](const auto& entry1, const auto& entry2) {
        return entry1.path().filename().string() < entry2.path().filename().string();
    });
    ostringstream tree_body;
    for (const auto& entry : entries)
    {
        // cout << entry.path() << "file=" << entry.is_regular_file() << '\n';
        if (entry.is_directory())
        {
            string mode = "40000";
            string name = entry.path().filename().string();
            string sha_bytes = _WriteTree(entry.path());
            tree_body << mode + " " + name + '\0' + sha_bytes;
        }
        else if (entry.is_regular_file())
        {
            auto perms = filesystem::status(entry).permissions();
            unsigned int perm_value = static_cast<unsigned int>(perms) & 0777;  // Mask for the permission bits
            string mode = "100" + to_string(perm_value); 
            string name = entry.path().filename().string();
            string blob = CreateBlobString(entry.path().string());
            string sha_bytes = ComputeShaHash(blob);
            tree_body << mode + " " + name + '\0' + sha_bytes;
            string sha = HexadecimalSha(sha_bytes);
            string compressedContent = compressContent(blob);
            
            storeCompressedFile(sha, compressedContent);
        }
    }
    string tree = "tree " + to_string(tree_body.str().size()) + '\0' + tree_body.str();
    string sha_bytes = ComputeShaHash(tree);
    string sha = HexadecimalSha(sha_bytes);
    string compressedContent = compressContent(tree);
            
    storeCompressedFile(sha, compressedContent);
    
    return sha_bytes;
}

int writeTree(string path){
    string sha_bytes = _WriteTree(path);
    string sha = HexadecimalSha(sha_bytes);
    cout << sha <<endl;
    return EXIT_SUCCESS;
}


pair<string, string> getUserInfo() {
    const char* homeDir = getenv("HOME");
    string path = string(homeDir) + "/.gitconfig";
    string gitConfigContent = readFile(path);
    string userName, userEmail;

    istringstream stream(gitConfigContent);
    string line;
    while (getline(stream, line)) {
        if (line.find("name = ") != string::npos) {
            userName = line.substr(line.find("name = ") + 7); // Extract the name value
        }
        if (line.find("email = ") != string::npos) {
            userEmail = line.substr(line.find("email = ") + 8); // Extract the email value
        }
    }



    return {userName, userEmail};
}

string getTimeStamp() {
    // Get current time as UNIX timestamp
    time_t now = time(nullptr);

    // Get the local time information, including timezone offset
    struct tm localTime;
    localtime_r(&now, &localTime);

    // Calculate the difference between local time and UTC in seconds
    time_t utcTime = mktime(gmtime(&now));
    // int timezoneOffsetSeconds = difftime(now, utcTime);
    int timezoneOffsetSeconds = localTime.tm_gmtoff;

    // Convert the offset to hours and minutes
    int timezoneOffsetHours = timezoneOffsetSeconds / 3600;
    int timezoneOffsetMinutes = (timezoneOffsetSeconds % 3600) / 60;

    // Format the timezone offset as ±HHMM
    ostringstream tzOffsetStream;
    tzOffsetStream << setfill('0') << setw(1) << (timezoneOffsetHours >= 0 ? '+' : '-')
                   << setw(2) << abs(timezoneOffsetHours) << setw(2) << abs(timezoneOffsetMinutes);

    // Format the timestamp with timezone offset
    ostringstream timestampStream;
    timestampStream << now << " " << tzOffsetStream.str();

    return timestampStream.str();
}

void updateHeadSHA(std::string& sha) {
    std::string headDir = ".git/refs/heads";
    std::string headFile = headDir + "/main";

    if (!fs::exists(headDir)) {
        fs::create_directories(headDir);
    }

    std::ofstream outFile(headFile, std::ios::trunc);
    if (!outFile) {
        throw std::runtime_error("Could not open file: " + headFile);
    }

    outFile << sha;
    outFile.close();
}

void commitTree(const string& flag1, const string& sha_parent, const string& flag2,
               const string& message, const string& sha)
{
    auto [userName, email] = getUserInfo();

    // Fetch current timestamp
    string unixTimestamp = getTimeStamp();
    cout<<"timestamp: "<<unixTimestamp<<endl;
    // Build the commit content
    ostringstream commitContent;
    commitContent << "tree " << sha << "\n";

    // Add parent if it exists
    if (!sha_parent.empty()) {
        commitContent << "parent " << sha_parent << "\n";
    }

    commitContent << "author " << userName << " <" << email << "> " << unixTimestamp << "\n";
    commitContent << "committer " << userName << " <" << email << "> " << unixTimestamp << "\n";
    commitContent << "\n" << message << "\n";

    // Create commit body
    string commitHeader = "commit " + to_string(commitContent.str().size()) + '\0';
    string commitBody = commitHeader + commitContent.str();

    // Get the commit SHA and hex representation
    string sha_bytes = ComputeShaHash(commitBody);
    string commit_sha = HexadecimalSha(sha_bytes);
    string compressedContent = compressContent(commitBody);
            
    storeCompressedFile(commit_sha, compressedContent);

    cout << commit_sha << "\n";  

    updateHeadSHA(commit_sha);

    // Log commit details
    string logDir = ".git/logs/refs/heads";
    string logFile = logDir + "/main";

    // Create the directory if it does not exist
    if (!fs::exists(logDir)) {
        fs::create_directories(logDir);
    }

    // Open the log file for appending (this will create the file if it does not exist)
    // Read existing log entries
    std::ifstream logStreamIn(logFile);
    std::string existingLogs((std::istreambuf_iterator<char>(logStreamIn)), std::istreambuf_iterator<char>());
    logStreamIn.close();

    // Open the log file for writing (this will overwrite the file)
    std::ofstream logStream(logFile, std::ios::trunc);
    if (!logStream) {
        throw runtime_error("Could not open log file: " + logFile);
    }

    // Write the new log entry at the top
    logStream << "commit " << commit_sha << " (HEAD -> main)\n"
              << "Author: " << userName << " <" << email << ">\n"
              << "Date:   " << unixTimestamp << "\n\n"
              << "    " << message << "\n\n";

    // Append the existing log entries
    logStream << existingLogs;

    logStream.close();
}

void printLogs() {
    string logFile = ".git/logs/refs/heads/main";
    ifstream file(logFile);
    if (!file.is_open()) {
        throw runtime_error("Unable to open log file: " + logFile);
    }

    string line;
    while (getline(file, line)) {
        cout << line << endl;
    }
    file.close();
}



// void addFiles(const string& path) {
//     map<string, string> fileMap;

//     function<void(const filesystem::path&)> iterateFiles = [&](const filesystem::path& dirPath) {
//         for (const auto& entry : filesystem::recursive_directory_iterator(dirPath)) {
//             // Normalize the path to remove . or ./ 
//             string normalizedPath = filesystem::absolute(entry.path()).lexically_normal().string();
//             cout<<normalizedPath<<endl;
//             // Skip unwanted files and directories
//             if (normalizedPath.find("/.git") != string::npos || 
//                 normalizedPath.find("/build") != string::npos || 
//                 normalizedPath.find("/vcpkg") != string::npos || 
//                 entry.path().filename() == "CMakeLists.txt" || 
//                 entry.path().filename() == ".DS_Store" ||
//                 entry.path().filename() == "vcpkg.json") {
//                 continue;
//             }
            
//             if (filesystem::is_regular_file(entry.path())) {
//                 string blobContent = CreateBlobString(normalizedPath);
//                 string sha1 = calculateSHA1(blobContent);
//                 string compressedContent = compressContent(blobContent);
//                 storeCompressedFile(sha1, compressedContent);
//                 fileMap[normalizedPath] = sha1;
//             }
//         }
//     };

//     // Handle if the input is a directory or file
//     if (filesystem::is_directory(path)) {
//         iterateFiles(path);
//     } else if (filesystem::is_regular_file(path)) {
//         filesystem::path fsPath(path);
//         string normalizedPath = filesystem::absolute(fsPath).lexically_normal().string();

//         // Skip specific files if the input is a file
//         if (normalizedPath.find("/.git") != string::npos || 
//             normalizedPath.find("/build") != string::npos || 
//             (filesystem::is_directory(fsPath) && fsPath.filename() == "vcpkg") || 
//             fsPath.filename() == "CMakeLists.txt" || 
//             fsPath.filename() == ".DS_Store") {
//             return;
//         }

//         string blobContent = CreateBlobString(normalizedPath);
//         string sha1 = calculateSHA1(blobContent);
//         string compressedContent = compressContent(blobContent);
//         storeCompressedFile(sha1, compressedContent);
//         fileMap[normalizedPath] = sha1;
//     } else {
//         cerr << "Invalid path: " << path << endl;
//     }

//     // Open the index file and append the file paths with SHA1 hashes
//     ofstream indexFile(".git/index", ios::app);
//     if (!indexFile) {
//         throw runtime_error("Could not open index file");
//     }

//     for (const auto& [filePath, fileSHA] : fileMap) {
//         indexFile << filePath << " " << fileSHA << "\n";
//     }

//     indexFile.close();
//     cout << "Files added to index." << endl;
// }

// void addFiles(vector<string>& paths) {
//     namespace fs = std::filesystem; // Ensure the filesystem namespace is included
//     map<string, string> fileMap;

//     auto processFile = [&](const fs::path& filePath) {
//         std::string absolutePath = fs::absolute(filePath).lexically_normal().string();
//         string blobContent = CreateBlobString(absolutePath);
//         string sha1 = calculateSHA1(blobContent);
//         string compressedContent = compressContent(blobContent);
//         storeCompressedFile(sha1, compressedContent);
//         fileMap[absolutePath] = sha1;
//     };

//     auto iterateFiles = [&](const fs::path& dirPath) {
//         for (const auto& entry : fs::recursive_directory_iterator(dirPath)) {
//             if ((entry.is_directory() && entry.path().filename() == ".git") || 
//                 (entry.is_directory() && entry.path().filename() == "build") || 
//                 (entry.is_directory() && entry.path().filename() == "vcpkg") || 
//                 entry.path().filename() == "CMakeLists.txt" || 
//                 entry.path().filename() == ".DS_Store") {
//                 continue;
//             }

//             if (fs::is_regular_file(entry.path())) {
//                 processFile(entry.path());
//             }
//         }
//     };

//     for (const auto& path : paths) {
//         if (fs::is_directory(path)) {
//             iterateFiles(path);
//         } else if (fs::is_regular_file(path)) {
//             processFile(path);
//         }
//     }
//     ofstream indexFile(".git/index", ios::app);
//     if (!indexFile) {
//         throw runtime_error("Could not open index file");
//     }

//     for (const auto& [filePath, fileSHA] : fileMap) {
//         indexFile << filePath << " " << fileSHA << "\n";
//     }

//     indexFile.close();
//     cout << "Files added to index." << endl;
// }


namespace fs = std::filesystem;

void addFiles(std::vector<std::string>& paths) {
    std::map<std::string, std::string> fileMap;

    auto processFile = [&](const fs::path& filePath) {
        std::string relativePath = fs::relative(filePath).string();
        std::string blobContent = CreateBlobString(relativePath);  // Assume this function creates blob content from file
        std::string sha1 = calculateSHA1(blobContent);  // Assume this calculates SHA1 of the file content
        std::string compressedContent = compressContent(blobContent);  // Assume this compresses the file content
        storeCompressedFile(sha1, compressedContent);  // Assume this stores the compressed file using its SHA1 hash
        fileMap[relativePath] = sha1;
    };

    auto iterateFiles = [&](const fs::path& dirPath) {
        for (fs::recursive_directory_iterator iter(dirPath, fs::directory_options::skip_permission_denied), end; iter != end; ++iter) {
            auto& entry = *iter;
            // Skip specific directories and files
            if (entry.is_directory()) {
                if (entry.path().filename() == ".git" || 
                    entry.path().filename() == "build" || 
                    entry.path().filename() == "vcpkg") {
                    iter.disable_recursion_pending();  // Prevent further recursion into these directories
                    continue;
                }
            }

            if (entry.path().filename() == "CMakeLists.txt" || entry.path().filename() == ".DS_Store") {
                continue;  // Skip specific files
            }

            if (fs::is_regular_file(entry.path())) {
                processFile(entry.path());
            }
        }
    };

    // Iterate over input paths and process them
    for (const auto& path : paths) {
        if (fs::is_directory(path)) {
            iterateFiles(path);  // Explore directory
        } else if (fs::is_regular_file(path)) {
            processFile(path);  // Process single file
        }
    }

    // Determine the mode to open the index file
    std::ios_base::openmode mode = std::ios::app;
    if (std::find(paths.begin(), paths.end(), ".") != paths.end()) {
        mode = std::ios::trunc;
    }

    // Write to index file
    std::ofstream indexFile(".git/index", mode);
    if (!indexFile) {
        throw std::runtime_error("Could not open index file");
    }

    for (const auto& [filePath, fileSHA] : fileMap) {
        indexFile << filePath << " " << fileSHA << "\n";
    }

    indexFile.close();
    std::cout << "Files added to index." << std::endl;
}


string _WriteTree(const fs::path& path, const std::map<std::string, std::string>& fileMap) {
    if (filesystem::is_empty(path))
    {
        return string("");
    }
    vector<filesystem::directory_entry> entries;
    for (const auto& entry : filesystem::directory_iterator(path))
    {
        if (entry.path().filename() == ".git" || entry.path().filename() == "build" || entry.path().filename() == "vcpkg" || entry.path().filename() == ".git" || entry.path().filename() == "CMakeLists.txt" || entry.path().filename() == ".DS_Store")
        {
            continue;
        }
        entries.push_back(entry);
    }
    sort(entries.begin(), entries.end(), [](const auto& entry1, const auto& entry2) {
        return entry1.path().filename().string() < entry2.path().filename().string();
    });
    ostringstream tree_body;
    for (const auto& entry : entries)
    {
        // cout << entry.path() << "file=" << entry.is_regular_file() << '\n';
        if (entry.is_directory())
        {
            string mode = "40000";
            string name = entry.path().filename().string();
            string sha_bytes = _WriteTree(entry.path(),fileMap);
            if (sha_bytes.empty())
            {
                continue;
            }
            
            tree_body << mode + " " + name + '\0' + sha_bytes;
        }
        else if (entry.is_regular_file())
        {
            std::string filePath = fs::relative(entry.path()).string();
            auto it = fileMap.find(filePath);
            if (it != fileMap.end()) {
                auto perms = filesystem::status(entry).permissions();
                unsigned int perm_value = static_cast<unsigned int>(perms) & 0777;  // Mask for the permission bits
                string mode = "100" + to_string(perm_value); 
                string name = entry.path().filename().string();
                string blob = CreateBlobString(entry.path().string());
                string sha_bytes = ComputeShaHash(blob);
                tree_body << mode + " " + name + '\0' + sha_bytes;
            }
        }
    }
    if(tree_body.str().empty()){
        return string("");
    }
    string tree = "tree " + to_string(tree_body.str().size()) + '\0' + tree_body.str();
    string sha_bytes = ComputeShaHash(tree);
    string sha = HexadecimalSha(sha_bytes);
    string compressedContent = compressContent(tree);
            
    storeCompressedFile(sha, compressedContent);
    
    return sha_bytes;
}

string getHeadSHA() {
    std::string headPath = ".git/refs/heads/main";
    if (fs::exists(headPath)) {
        std::ifstream headFile(headPath);
        if (headFile.is_open()) {
            std::string sha;
            std::getline(headFile, sha);
            headFile.close();
            return sha;
        } else {
            std::cerr << "Error: Unable to open file " << headPath << std::endl;
            return "";
        }
    } else {
        return "";
    }
}

void commit(std::string& indexPath, std::string& message) {
    std::ifstream indexFile(indexPath);
    if (!indexFile) {
        throw std::runtime_error("Could not open index file");
    }

    std::map<std::string, std::string> fileMap;
    std::string line;
    while (std::getline(indexFile, line)) {
        std::istringstream iss(line);
        std::string filePath, fileSHA;
        if (!(iss >> filePath >> fileSHA)) {
            continue;
        }
        fileMap[filePath] = fileSHA;
    }
    indexFile.close();

    std::string rootPath = ".";
    std::string treeSHA = _WriteTree(rootPath, fileMap);
    string sha = HexadecimalSha(treeSHA);
    string headSha = getHeadSHA();
    commitTree("-p", headSha, "-m", message, sha);
    // updateHeadSHA(sha);

    // Delete and flush the index file
    // std::ofstream ofs(indexPath, std::ios::trunc);
    // if (!ofs) {
    //     throw std::runtime_error("Could not open index file to clear it");
    // }
    // ofs.close();
}

std::string decompressContent(const std::string& compressedContent) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    if (inflateInit(&zs) != Z_OK) {
        throw std::runtime_error("inflateInit failed while decompressing.");
    }

    zs.next_in = (Bytef*)compressedContent.data();
    zs.avail_in = compressedContent.size();

    int ret;
    char outbuffer[32768];
    std::string decompressedContent;

    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);

        ret = inflate(&zs, 0);

        if (decompressedContent.size() < zs.total_out) {
            decompressedContent.append(outbuffer, zs.total_out - decompressedContent.size());
        }
    } while (ret == Z_OK);

    inflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        throw std::runtime_error("Exception during zlib decompression: (" + std::to_string(ret) + ") " + zs.msg);
    }

    return decompressedContent;
}

std::string readObject(const std::string& sha) {
    std::string objectDir = ".git/objects/" + sha.substr(0, 2);
    std::string objectFile = objectDir + "/" + sha.substr(2);

    std::ifstream inFile(objectFile, std::ios::binary);
    if (!inFile) {
        throw std::runtime_error("Could not open object file: " + objectFile);
    }

    std::ostringstream ss;
    ss << inFile.rdbuf();
    std::string compressedContent = ss.str();
    inFile.close();

    return decompressContent(compressedContent);
}

void extractTree(const std::string& treeSHA, const fs::path& basePath) {
    std::string treeContent = readObject(treeSHA);

    // Remove the header from treeContent
    size_t nullPos = treeContent.find('\0');
    if (nullPos != std::string::npos) {
        treeContent = treeContent.substr(nullPos + 1);
    }

    size_t i = 0;
    while (i < treeContent.size()) {
        // Extract mode
        std::string mode;
        while (treeContent[i] != ' ') {
            mode += treeContent[i];
            i++;
        }
        i++; // Skip the space

        // Extract filename
        std::string filename;
        while (treeContent[i] != '\0') {
            filename += treeContent[i];
            i++;
        }
        i++; // Skip the null byte

        // Extract SHA-1 hash (next 20 bytes)
        std::string sha = to_hex_string(reinterpret_cast<const unsigned char*>(&treeContent[i]), 20);
        i += 20;

        fs::path filePath = basePath / filename;
        if (mode == "40000") { // Directory
            fs::create_directories(filePath);
            extractTree(sha, filePath);
        } else if (mode.substr(0, 3) == "100") { // File
            std::string blobContent = readObject(sha);

            // Remove the header from blobContent
            size_t nullPos = blobContent.find('\0');
            if (nullPos != std::string::npos) {
                blobContent = blobContent.substr(nullPos + 1);
            }

            std::ofstream outFile(filePath, std::ios::binary);
            outFile << blobContent;
            outFile.close();
        }
    }
}

void removeAllExceptGit() {
    for (const auto& entry : fs::directory_iterator(".")) {
        if (entry.path().filename() == ".git" || entry.path().filename() == "build" || 
            entry.path().filename() == "vcpkg" || entry.path().filename() == "CMakeLists.txt" || 
            entry.path().filename() == ".DS_Store") {
            continue;
        }
        fs::remove_all(entry.path());
    }
}

void extractCommit(string& commitSHA) {
    removeAllExceptGit(); // Remove all files and directories except specified ones

    std::string commitContent = readObject(commitSHA);
    cout<<commitContent<<endl;
    size_t nullPos = commitContent.find('\0');
    if (nullPos != std::string::npos) {
        commitContent = commitContent.substr(nullPos + 1);
    }
    std::istringstream iss(commitContent);
    std::string line;
    std::string treeSHA;

    while (std::getline(iss, line)) {
        if (line.substr(0, 5) == "tree ") {
            treeSHA = line.substr(5);
            break;
        }
    }

    if (treeSHA.empty()) {
        throw std::runtime_error("No tree SHA found in commit.");
    }

    fs::path basePath = ".";
    extractTree(treeSHA, basePath);
}