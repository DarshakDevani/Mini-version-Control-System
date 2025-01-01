# Version Control System
This project is a custom-built version control system written in C++, similar in functionality to Git. It allows users to manage file versions. 


### Files 
- Server.cpp
- utils.cpp
- utils.h
- main_program.sh

### How to run
```
    main_program.sh <git-command>
```
### Supported Commands

1. **init**
 - Sets up necessary subdirectories and configuration files to keep track of the project’s history and changes. 
- This initialization is crucial, as it sets up the environment for further commands to operate.
    ### Example
    ```
        main_program.sh init
    ```
- A hidden directory named .git is created in the root of your project. This directory contains all the necessary files and subdirectories that is used to manage version control.
---
2. **hash-object**
 - The hash-object command  is used to create a unique identifier(hash) for a given file’s contents.
    ### Example
    ```
        main_program.sh hash-object -w(optional) relative-filepath
    ```
- The output will be a 40-character SHA-1 hash value that represents the file's content, including a header with metadata like object type and size.
- If the `-w` option is set, the command will store the file's content as an object in the `./.git/objects` folder.
- The content will be stored in a compressed format using Zlib compression at the default level of 6.
---

3. **cat-file**
- The cat-file command is used to display the content/type/size of a specific object (such as a file or tree) stored in the repository. 
    ### Example 
    ```
    ./main_program.sh cat-file -p <hash>
    ./main_program.sh cat-file -t <hash>
    ./main_program.sh cat-file -s <hash>
    ```
- -p: Outputs the content of the object in a readable format.
-  -t: Displays the type of the object (e.g., blob for file content, tree for directory structure, commit for commit object)..
-  -s: Displays the size of the content in bytes.

---

4. **write-tree**
- The write-tree command constructs a "tree" object representing the current state of the working directory.
- This tree object captures the structure and contents of all tracked files and subdirectories, creating a snapshot of the entire project hierarchy.
    ### Example 
    ```
    ./main_program.sh write-tree
    ```
- The output is a 40-character SHA-1 hash, representing the root tree object for the current directory structure.

---

5. **ls-tree**
 - The ls-tree command is used to display the contents of a tree object in a readable format.
 - This command is useful for examining the structure of a specific directory snapshot within the repository, allowing you to view all files and subdirectories along with their metadata (such as type, permission and hash).
    ### Example 
    ```
    ./main_program.sh ls-tree <tree-hash>
    ```
---

6. **commit-tree**

- The commit-tree command is used to create a new commit object in the version control system. 
- This command captures the current state of a tree object and associates it with a commit message and metadata such as the author and timestamp.
    ### Example
    ```
    1] ./my_program.sh commit-tree <tree-hash> -m "Commit Message"
    2] ./my_program.sh commit-tree <tree-hash> -p <parent-tree-hash> -m "Commit Message"
    ```
- The output is a 40-character SHA-1 hash that indicates a new commit object has been created with the specified commit message and stored in the `./.git/objects` folder.
- The -p (parent) flag in the commit-tree command allows you to specify one or more parent commit hashes when creating a new commit object.     

---

7. **add**

- The add command is used to stage changes in the working directory for the next commit. This command allows users to specify which files or directories should be included in the upcoming commit.
    ### Example
    ```
    ./main_program.sh add <list of file-path>
    ./main_program.sh add <directory-path>  # Stages all files in the specified directory
    ./main_program.sh add .                 # Stages all changes in the current directory
    ```
- The add command places changes in a staging area(index file), which acts as a buffer before the actual commit.

---

8. **commit**

- The commit command is used to save the staged changes in the staging area to the repository, creating a new commit object. 
    ### Example
    ```
    ./main_program.sh commit -m "Your commit message here"
    ```
- Commit Object Structure: Each commit object includes:
    - A pointer to the corresponding tree object representing the staged files.
    - The commit message.
    - Metadata such as author information and timestamp.
    - Links to parent commits if applicable
- The commit is identified by a 40-character SHA-1 hash, generated based on the content of the commit and its metadata, ensuring unique identification of each commit.

---

9. **log**

- The log command is used to display the commit history of the repository. 
- This command provides a chronological list of commits, allowing users to review past changes, understand the evolution of the project, and track contributions over time.
    ### Example
    ```
    ./main_program.sh log
    ```
- The output will include the following commit information for each entry in the log:

    - **Commit Hash**: A unique identifier for the commit.
    - **Author Information**: The name and email of the author who made the commit.
    - **Timestamp**: The date and time when the commit was created.
    - **Commit Message**: A description of the changes made in that commit.

- Commits are displayed in reverse chronological order, with the most recent commit listed first.

---

10. **Checkout**

- The checkout command is used to switch between different commits. This command allows users to navigate the project history, work on different features, or revert changes to specific files.

    ### Example
    ```
    ./main_program.sh checkout <commit-sha>
    ```
- The files in your working directory are replaced with their versions from the specified commit. This means any modifications made after that commit will be lost unless they have been saved elsewhere (e.g., committed).