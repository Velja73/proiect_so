#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>

typedef struct Metadata {
    char name[256];
    char path[512];
    char type;
    off_t size;
    time_t last_modification;
}Metadata;


void parse_directories(char *path,  Metadata *metadate) {

    struct stat fileStat;
    if (stat(path, &fileStat) < 0){
        perror("Stat not working error");
        return;
    }
    strcpy(metadate->name, strrchr(path, '/') + 1);
    strcpy(metadate->path, path);
    metadate->type = (S_ISDIR(fileStat.st_mode)) ? 'D' : 'F';
    metadate->size = fileStat.st_size;
    metadate->last_modification = fileStat.st_mtime;
}

void move_file(char *file_path, char *isolated_path) {

    if (access(file_path, F_OK) != 0) {
        printf("File %s do not exists.\n", file_path);
        return;
    }
    printf("%s should be moved\n", file_path);
    int pipe_fd[2];
    pid_t pid;
    char aux_buffer[1024];

    if (pipe(pipe_fd) == -1) {
        perror("Eroare at creating the pipe");
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid == -1) {
        perror("Eroare at creating child process");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        close(pipe_fd[0]);

        char *args[] = {"./analizeaza_fisier.sh", file_path, NULL};
        dup2(pipe_fd[1], STDOUT_FILENO); // stdout to pipe
        execvp(args[0], args);

        // Dacă execvp() a ajuns pana aici
        perror("Error at running the script");
        exit(EXIT_FAILURE);
    } else {
        close(pipe_fd[1]);

        int nbytes = read(pipe_fd[0], aux_buffer, sizeof(aux_buffer));
        if (nbytes == -1) {
            perror("Can't read from pipe");
            exit(EXIT_FAILURE);
        }
        aux_buffer[nbytes] = '\0';

        if (strcmp(aux_buffer, "CORUPT\n") == 0) {
            printf("Corrupted file: %s\n", file_path);
            char file_name[512];
            sprintf(file_name, "%s/%s", isolated_path, strrchr(file_path, '/') + 1);
            if (rename(file_path, file_name) == -1) {
                perror("Renaming file error\n");
                exit(EXIT_FAILURE);
            }
            printf("The file \"%s\" has been isolated in \"%s\".\n", file_path, isolated_path);
        } else if (strcmp(aux_buffer, "SIGUR\n") == 0) {
            printf("Safe file: %s\n", file_path);
        }

        close(pipe_fd[0]);
    }
}

void snapshot(char *dir_path, char *snap_path, char *isolated_path) {
    int fp;
    Metadata metadate;
    DIR *dir;
    struct dirent *entry;

    struct stat st;
    if (stat(snap_path, &st) == -1) {
        mkdir(snap_path, 0777);//daca nu exista, creez unul
    }

    if ((dir = opendir(dir_path))) {
        char old_snap[512], new_snap[512];
        sprintf(old_snap, "%s/OldSnapshot.txt", snap_path);
        sprintf(new_snap, "%s/NewSnapshot.txt", snap_path);
        fp = open(new_snap, O_WRONLY | O_CREAT | O_TRUNC, 0644);

        while ((entry = readdir(dir))) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                char file_path[512];
                sprintf(file_path, "%s/%s", dir_path, entry->d_name);
                parse_directories(file_path, &metadate);
                dprintf(fp, "%s\t%s\t%c\t%ld\t%s", metadate.name, metadate.path, metadate.type, metadate.size, ctime(&metadate.last_modification));
                printf("Analyzing the file : %s\n", file_path);
                if (metadate.type == 'F') {
                    move_file(file_path, isolated_path);
                }
            }
        }
        close(fp);
        closedir(dir);
    } else {
        perror("Error: can't open the dir");
        exit(EXIT_FAILURE);
    }
}

void write_mod_time(const char* filename, time_t modTime) {
    FILE *f;
    if ((f = fopen(filename, "w")) == NULL) {
        perror("\nOops, eroare la deschiderea fisierului\n");
        exit(-1);
    }
    fprintf(f, "%ld", (long)modTime);
    if(fclose(f) != 0)
    {
        perror("\nOops, eroare la inchiderea fisierului\n");
        exit(-1);
    }
}


int main(int argc, char **argv) {
    char old_snap[256], new_snap[256];
    
    if (argc < 6 || argc > 14 || strcmp(argv[1], "-o") != 0 || strcmp(argv[3], "-s") != 0) {
        printf("Invalid options. Do this: %s -o out_dir -s isolated_dir dir1 dir2 ... dir 10", argv[0]);
        exit(EXIT_FAILURE);
    }

    sprintf(old_snap, "%s/OldSnapshot.txt", argv[2]);
    sprintf(new_snap, "%s/NewSnapshot.txt", argv[2]);
   
    if (access(old_snap, F_OK) != -1) {
        // Compară snapshot-uri
        printf("Comparing the snapshots:\n");
       
    } else {
        printf("There is no old snapshot. It's running for the first time.\n");
    }
 // Creează un nou snapshot
    printf("Create a new snapshot:\n");
    snapshot(argv[5], new_snap, argv[4]);

    return 0;
}
