int cd(char *pathname);
int ls(char *pathname);
int touch(char *filename);
int my_mkdir(char *filename);

char *commands[] = {
    "cd",
    "ls",
    "touch",
    "mkdir",
};

int (* exec_command[])(char *) = {
    &cd,
    &ls,
    &touch, 
    &my_mkdir,
};
