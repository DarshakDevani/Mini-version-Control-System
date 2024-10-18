// #ifndef HEADERS_H
// #define HEADERS_H

// using namespace std;
// // Declare a function
// void decompressGitObjectChunkwise(const string& filepath);
// string getFilePathFromSHA(const string& sha);
// string readFile(const string &filename);
// string calculateSHA1(const string &input);
// string compressContent(const string &content);
// void storeCompressedFile(const string &sha1, const string &compressedContent);


// #endif 

#ifndef HEADERS_H
#define HEADERS_H

#include <string> // Include the string header
#include <iostream> // Include iostream if using cout or other I/O

using namespace std; // Use the entire standard namespace

// Declare functions
void decompressGitObjectChunkwise(const string& filepath,string hash,string objectFlag);
string getFilePathFromSHA(const string& sha);
string readFile(const string& filename);
string calculateSHA1(const string& input);
string compressContent(const string& content);
void storeCompressedFile(const string& sha1, const string& compressedContent);
string to_hex_string(const unsigned char *data, size_t length);
void parse_tree_object(vector<unsigned char> &data, bool option);
bool read_and_decompress_git_object(const string &sha1_hash,bool flag, const string &git_dir = ".git");
int writeTree(string path);
void commitTree(const string& flag1, const string& sha_parent, const string& flag2,
               const string& message, const string& sha);
void addFiles(vector<string>& paths);
void commit(std::string& indexPath, std::string& message);
void printLogs();
void extractCommit(string& commitSHA);
#endif // MY_FUNCTIONS_H
