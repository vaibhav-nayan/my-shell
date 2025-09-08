# 🐚 My Unix-like Shell  

A custom **Unix-like command-line shell** written in **C**, built from scratch as a learning project to explore operating systems, process management, and CLI design.  

It supports core shell functionalities like **pipelines, I/O redirection, background jobs, job control, and history expansion**, while integrating **GNU Readline** for a modern user experience with persistent history and tab completion.  

---

## ✨ Features  

- **Command Execution**
  - Run external programs like a normal shell (`ls`, `cat`, etc.).  
- **Built-ins**
  - `cd`, `pwd`, `exit`, `jobs`, `fg`, `bg`, `history`.  
- **I/O Redirection**
  - Input `<`, Output `>`, Append `>>`.  
- **Pipelines**
  - Multi-stage pipelines (`cat file | grep foo | sort`).  
- **Background Jobs**
  - Run processes in the background (`sleep 10 &`) and track with `jobs`.  
- **Job Control**
  - Bring jobs to foreground (`fg %id`), resume stopped jobs (`bg %id`).  
- **Signals**
  - Handles `Ctrl+C` (SIGINT) and `Ctrl+Z` (SIGTSTP) like real shells.  
- **History Expansion**
  - `!!` (last command), `!n` (nth command).  
- **GNU Readline Integration**
  - Persistent history across sessions.  
  - Tab completion for commands.  
- **Environment Variable Expansion**
  - Use variables (`export foo=bar`, then `echo $foo`).  
- **Configurable Prompt**
  - Shows user and working directory.  
- **Deployment Ready**
  - Dockerized for consistent environment.  
  - GitHub Actions CI for automated build & test.  

---

## 🚀 Getting Started  

### 1. Clone the repository  
```bash
git clone https://github.com/yourusername/my_shell.git
cd my_shell
```

### 2. Local Build & Run  

Make sure you have **GCC** and **GNU Readline** installed:  
```bash
sudo apt-get update
sudo apt-get install build-essential libreadline-dev
```

Build:  
```bash
make
```

Run:  
```bash
./my_shell
```

Clean build artifacts:  
```bash
make clean
```

---

## 🐳 Run with Docker  

You can run the shell inside Docker (no dependencies on your host machine).  

### Build image:  
```bash
docker build -t my_shell .
```

### Run container:  
```bash
docker run -it my_shell
```

---

## 📖 Demo  

### Run external commands  
```bash
ls -l
pwd
```

### Pipelines  
```bash
cat file.txt | grep foo | sort
```

### I/O Redirection  
```bash
echo "hello" > out.txt
cat < out.txt
```

### Background jobs & job control  
```bash
sleep 10 &
jobs
fg %1
```

### History Expansion  
```bash
!!    # repeat last command
!3    # run 3rd command in history
```

### Environment variables  
```bash
export foo=bar
echo $foo
unset foo
```

---

## 🛠 Project Structure  

```
my_shell/
├── docs/
├── include
├── src/
│   └── main.c         # Main shell implementation
├── tests/ 
├── Makefile           # Build instructions
├── my_shell           # main executable file
├── Dockerfile         # Docker setup
├── .github/workflows/ # CI/CD configs
└── README.md          # Project documentation
```

---

## 📚 Learning Outcomes  

- Hands-on understanding of **process creation (fork, exec)**, **signals**, **pipes**, and **file descriptors**.  
- Exposure to **job control** and how shells manage foreground/background processes.  
- Practical use of **GNU Readline** for command-line interfaces.  
- Deployment best practices with **Docker & CI/CD pipelines**.  

---

## 🧑‍💻 Author  

**Vaibhav Nayan**  
[LinkedIn](https://www.linkedin.com/in/vaibhav-nayan) · [GitHub](https://github.com/vaibhav-nayan)  
