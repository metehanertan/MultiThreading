#include <unistd.h>     /* Symbolic Constants */
#include <sys/types.h>  /* Primitive System Data Types */
#include <errno.h>      /* Errors */
#include <stdio.h>      /* Input/Output */
#include <stdlib.h>     /* General Utilities */
#include <pthread.h>    /* POSIX Threads */
#include <string.h>     /* String handling */
#include <semaphore.h>  /* Semaphore */

/*********************************Function Prototypes********************************/
int lineCounter(char inputFile[]);

void *lineReader(void *threadid);

void *upper(void *args);

void *und(void *args);

int isThereLineToUp();

int isThereLineToUnd();

void fullOutput();

void copyTempToOut();

void *writeToText(void *args);

int isThereLineToWrite();

/*********************************Function Prototypes********************************/

typedef struct Reader {
    long threadid;
    char input[];
} Reader;

#define MAX_LINE_LENGTH 256
#define MAX_NUMBER_OF_LINES 1000
#define MAX_NUMBER_OF_THREADS 1000

sem_t mutexRead;
sem_t mutexChange;
sem_t *lineLocks[MAX_NUMBER_OF_LINES];
sem_t mutexWrite;
int counter = 0;
int totalLine;
int undCOunter = 0;
int upCounter = 0;
int writeCounter = 0;
int readCount = 0;
char *readed[MAX_NUMBER_OF_LINES];
pthread_t threads[MAX_NUMBER_OF_THREADS];
/***************************************************/
int lock[MAX_NUMBER_OF_LINES];
/***** default value is 3. three means that line is not read yet.
 * When upper thread finish his job it decrements the value by one.
 * When underscore thread finish his job it increments the value by two.
 * if the value is -1 it means that line is only read.
 * if the value is -2 that means that line is read and a upper thread do his job on this line.
 * if the value is 1 that meas that line is read and a underscore thread do his job on this line.
 * if the value is 0 that means we are done with this thread we can write this thread.
 * after we write it to the file we make its value 3 again.
 */
/***************************************************/
char *name;
int main(int argc, char *argv[]) {
    if(argc != 6) {
        printf("invalid number of parameters!\n");
        exit(1);
    }

    totalLine = lineCounter(argv[1]);
    int readers = *argv[2] - '0';
    int uppers = *argv[3] - '0';
    int unders = *argv[4] - '0';
    int writers = *argv[5] - '0';
    int totalThreads = readers + uppers + unders + writers;
    printf("total number of line is : %d\n", totalLine);
    name = malloc(50 * sizeof(char));
    strcpy(name, argv[1]);
    for (int i = 0; i < totalLine; i++) {
        readed[i] = malloc(MAX_LINE_LENGTH * sizeof(char));
        lock[i] = 3;
    }

    sem_init(&mutexRead, 0, 1);
    sem_init(&mutexChange, 0, 1);
    sem_init(&mutexWrite, 0, 1);

    for (int i = 0; i < totalLine; i++) {
        lineLocks[i] = malloc(sizeof(sem_t));
        sem_init(lineLocks[i], 0, 1);
    }

    for (int i = 0; i < uppers; i++) {
        int *th = (int *) malloc(sizeof(int));
        *th = i;
        pthread_create(&threads[i + readers], NULL, &upper, (void *) th);
    }

    for (int i = 0; i < unders; i++) {
        int *th = (int *) malloc(sizeof(int));
        *th = i;
        pthread_create(&threads[i + readers + uppers], NULL, &und, (void *) th);
    }

    for (int i = 0; i < writers; i++) {
        int *th = (int *) malloc(sizeof(int));
        *th = i;
        pthread_create(&threads[i + readers + uppers + writers], NULL, &writeToText, (void *) th);
    }

    for (int i = 0; i < readers; i++) {
        Reader *thread = malloc(sizeof(Reader));
        thread->threadid = 1;
        strcpy(thread->input, name);
        pthread_create(&threads[i], NULL, &lineReader, (void *) thread);
    }


    for (int i = 0; i < totalThreads; i++)
        pthread_join(threads[i], NULL);

    sem_destroy(&mutexRead); /* destroy semaphore */
    return 0;
}

int lineCounter(char inputFile[]) {//This function counts the lines and returns the total number of lines
    FILE *file;
    char ch;
    int lines = 0;
    file = fopen(inputFile, "r");

    if (file == NULL) {
        printf("\nUnable to open file.\n");
        printf("Please check if file exists and you have read privilege.\n");

        exit(EXIT_FAILURE);
    }

    while ((ch = fgetc(file)) != EOF) {

        if (ch == '\n' || ch == '\0')
            lines++;
    }

    fclose(file);
    return lines;
}

void *lineReader(void *reader) {//This function is for read threads

    while (counter < totalLine) {
        sem_wait(&mutexRead);//in here we lock the picking line mechanism to prevent two different thread to choose same line
        int x = counter;
        counter++;
        readCount++;
        if(readCount == 1)
            sem_wait(&mutexWrite);
        sem_post(&mutexRead);

        FILE *file;
        char ch;
        int lines = 0;
        file = fopen(((Reader *) reader)->input, "r");

        if (file == NULL) {
            printf("\nUnable to open file.\n");
            printf("Please check if file exists and you have read privilege.\n");

            exit(EXIT_FAILURE);
        }
        char *in = (char *) malloc(MAX_LINE_LENGTH * sizeof(char));
        size_t size = MAX_LINE_LENGTH;

        for (int i = 0; i < x; i++)
            getline(&in, &size, file);
        getline(&in, &size, file);
        /*if (x == totalLine - 1)
            strcat(in, "\n");*/
        printf("Hello im %dth Tread and I read %dth line: %s", ((Reader *) reader)->threadid, x, in);
        strcpy(readed[x], in);

        sem_wait(&mutexRead);
        readCount--;
        if(readCount == 0)
            sem_post(&mutexWrite);
        sem_post(&mutexRead);

        sem_wait(&mutexChange);//We need to change lock array to let our up and und threads that lines is available
        lock[x] = -1;
        sem_post(&mutexChange);
    }

    pthread_exit(NULL);
}

void *upper(void *args) {
    int line;
    while (upCounter < totalLine) {
        sem_wait(&mutexChange);
        line = isThereLineToUp();
        if (line >= 0) {
            upCounter++;
            lock[line] -= 1;
            sem_wait(&mutexWrite);
        }
        sem_post(&mutexChange);

        if (line == -1) {
            continue;
        }


        sem_wait(lineLocks[line]);
        printf("Im Upper Thread no : %d and changing line no : %d\n", *((int *) args), line);
        for (int i = 0; i < MAX_LINE_LENGTH; i++) {
            if (readed[line][i] >= 'a' && readed[line][i] <= 'z')
                readed[line][i] = readed[line][i] - 'a' + 'A';
        }
        sem_post(lineLocks[line]);
        sem_post(&mutexWrite);
    }

    pthread_exit(NULL);
}

void *und(void *args) {

    int line;
    while (undCOunter < totalLine) {

        sem_wait(&mutexChange);
        line = isThereLineToUnd();
        if (line >= 0) {
            undCOunter++;
            lock[line] += 2;
            sem_wait(&mutexWrite);
        }
        sem_post(&mutexChange);

        if (line == -1) {
            continue;
        }


        sem_wait(lineLocks[line]);
        printf("Im Underscore Thread no : %d and changing line no : %d\n", *((int *) args), line);
        for (int i = 0; i < MAX_LINE_LENGTH; i++) {
            if (readed[line][i] == ' ')
                readed[line][i] = '_';
        }
        sem_post(lineLocks[line]);
        sem_post(&mutexWrite);
    }

    pthread_exit(NULL);
}

int isThereLineToUp() {

    for (int i = 0; i < totalLine; i++) {
        if (lock[i] == 3)
            continue;
        if (lock[i] == -1 || lock[i] == 1)
            return i;
    }

    return -1;
}

int isThereLineToUnd() {

    for (int i = 0; i < totalLine; i++) {
        if (lock[i] == 3)
            continue;
        if (lock[i] < 0)
            return i;
    }

    return -1;
}

int isThereLineToWrite() {

    for (int i = 0; i < totalLine; i++) {
        if (lock[i] == 3)
            continue;
        if (lock[i] == 0)
            return i;
    }
    return -1;
}

void *writeToText(void *args) {

    char str[MAX_LINE_LENGTH];
    int line;

    while (writeCounter < totalLine) {
        int i = 0;
        sem_wait(&mutexChange);
        line = isThereLineToWrite();
        if (line >= 0) {
            writeCounter++;
            lock[line] = 3;
        }
        sem_post(&mutexChange);

        if (line == -1) continue;

        sem_wait(&mutexWrite);
        FILE *fp;                                   //Opening output text file
        fp = fopen(name, "r");
        if (fp == NULL)
            exit(10);

        FILE *ftemp;                                //Opening temporary text file
        ftemp = fopen("temp.txt", "w");
        if (ftemp == NULL)
            exit(11);

        while (i < totalLine) {                    //Copying output to temporary file also adding new text
            fgets(str, sizeof str, fp);
            if (i == line) {
                fprintf(ftemp, "%s", readed[line]);
            } else {
                fprintf(ftemp, "%s", str);
            }
            i++;
        }

        fclose(ftemp);
        fclose(fp);
        printf("Im Write Thread no : %d and writing line no : %d\n", *((int *) args), line);
        copyTempToOut();
        sem_post(&mutexWrite);
    }
    pthread_exit(NULL);
}

//Coppies the temp text to output text and deletes the temp text
void copyTempToOut() {

    char str[MAX_LINE_LENGTH];
    int i = 0;

    FILE *fp;                                   //Opening output text file
    fp = fopen(name, "w");
    if (fp == NULL)
        exit(12);

    FILE *ftemp;                                //Opening temporary text file
    ftemp = fopen("temp.txt", "r");
    if (ftemp == NULL)
        exit(13);

    for (i = 0; i < totalLine; i++) {              //Copying temp text to output text
        fgets(str, sizeof str, ftemp);
        fprintf(fp, "%s", str);
    }

    fclose(ftemp);
    fclose(fp);
    remove("temp.txt");
}