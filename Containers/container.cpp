#include <iostream>
#include <sched.h>
#include <sys/wait.h>
#include <unistd.h>
#include<sys/mount.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <fstream>


#define MEMORY_ALLOC_ERROR "system error: memory allocation failed\n"
#define SETHOSTNAME_ERROR "system error: sethostname call function was failed\n"
#define CHROOT_ERROR "system error: chroot call function was failed\n"
#define MKDIR_ERROR "system error: mkdir call function was failed\n"
#define MOUNT_ERROR "system error: mount call function was failed\n"
#define UNMOUNT_ERROR "system error: unmount call function was failed\n"
#define EXECVP_ERROR "system error: execvp call function was failed\n"
#define CLONE_ERROR "system error: failed to create a new container\n"
#define OPENING_FILE_ERROR "system error: failed to open the file\n"
#define STACK_SIZE 8192
#define PERMISSIONS 0755

#define FLAGS CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | SIGCHLD
#define TOP_FOLDER "/sys/fs/cgroup/pids/container/"

void systemCallFailed(const std::string& str)
{
  std::cerr << str;
  exit(EXIT_FAILURE);
}

char *stackMemoryAllocation(void *stack)
{
  stack = malloc(STACK_SIZE);
  if (stack == nullptr)
    systemCallFailed(MEMORY_ALLOC_ERROR);
  return (char *) stack + STACK_SIZE;
}

void writeToFile(const std::string& filePath, const char* str) {
  std::fstream f;
  f.open(filePath, std::ios::out);
  if (!f)
    systemCallFailed(OPENING_FILE_ERROR);
  f << str;
  f.close();
}


int program(void *prevArgv)
{
  char **argv = (char **) prevArgv;

  auto hostName = argv[1];
  auto fileSystemDirectory = argv[2];
  auto numProcesses = argv[3];
  auto pathToProgram = argv[4];
  auto argsForProgram = argv[5];

  // set hostName after CLONE_NEWUTS flag, not sure about the checking validation.
  if (sethostname(hostName, strlen(hostName)) < 0)
    systemCallFailed(SETHOSTNAME_ERROR);

  // changing the root to a new directory.
  if (chroot(fileSystemDirectory) < 0)
    systemCallFailed(CHROOT_ERROR);
  chdir(fileSystemDirectory);

  // mount the proc directory.
  if (mount("proc", "/proc", "proc", 0, 0)  < 0 )
    systemCallFailed(MOUNT_ERROR);

  // limiting the number of processes.
  if (mkdir("/sys/fs", PERMISSIONS) < 0 || mkdir("/sys/fs/cgroup", PERMISSIONS) < 0 ||
  mkdir("/sys/fs/cgroup/pids", PERMISSIONS) < 0){
    systemCallFailed(MKDIR_ERROR);}


  const char* pid  = std::to_string(getpid()).c_str();
  writeToFile("/sys/fs/cgroup/pids/cgroup.procs", pid);
  chmod("/sys/fs/cgroup/pids/cgroup.procs", PERMISSIONS);
  writeToFile("/sys/fs/cgroup/pids/pids.max", numProcesses);
  chmod("/sys/fs/cgroup/pids/pids.max", PERMISSIONS);
  writeToFile("/sys/fs/cgroup/pids/notify_on_release", "1");
  chmod("/sys/fs/cgroup/pids/notify_on_release", PERMISSIONS);


  // executing the program.
  if (execvp(pathToProgram, argv + 4) < 0)
    systemCallFailed(EXECVP_ERROR);

  // unmount the container root.
  if (umount("/proc") < 0 )
    systemCallFailed(UNMOUNT_ERROR);

  return EXIT_SUCCESS;
}


int main(int argc, char **argv)
{
  char *stack = nullptr;
  
  char** newArgv = (char**)malloc((argc + 1) * sizeof(char*));
  if (!newArgv)
    systemCallFailed(MEMORY_ALLOC_ERROR);
  std::copy(argv, argc + argv, newArgv);
  newArgv[argc] = nullptr;

  if (clone(program, stackMemoryAllocation(stack), FLAGS, newArgv) < 0)
    systemCallFailed(CLONE_ERROR);

  wait(nullptr);

  std::string buf("rm -rf ");
  buf.append(argv[2]);
  buf.append("/sys/fs");
  system(buf.c_str());
  std::string path(argv[2]);
  path.append("/proc");
  if (umount(path.c_str())  < 0){
    systemCallFailed(UNMOUNT_ERROR);
  }
  free(stack);
  free(newArgv);
  return EXIT_SUCCESS;
}
