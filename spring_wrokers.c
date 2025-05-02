#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>  // For sleep and usleep

// Constants
#define MAX_FRUITS 30     // Total number of fruits on the tree
#define CRATE_SIZE 12     // Maximum size of crate
#define PICKER_COUNT 3    // Number of pickers (threads)

// Shared data
int tree[MAX_FRUITS];     // Tree as an array of fruits
int tree_size = MAX_FRUITS; // Number of fruits currently on the tree
int crate[CRATE_SIZE];    // Crate to store picked fruits
int crate_count = 0;      // How many fruits are currently in the crate

// Threading and synchronization variables
pthread_t pickers[PICKER_COUNT];    // Picker threads
pthread_t loader_thread;            // Loader thread

pthread_mutex_t tree_mutex = PTHREAD_MUTEX_INITIALIZER;     // Mutex for tree access
pthread_mutex_t crate_mutex = PTHREAD_MUTEX_INITIALIZER;    // Mutex for crate access
pthread_cond_t crate_full = PTHREAD_COND_INITIALIZER;       // Condition: crate is full
pthread_cond_t crate_emptied = PTHREAD_COND_INITIALIZER;    // Condition: crate is emptied

// Function to initialize the tree with fruits (fruit IDs)
void init_tree() {
    for (int i = 0; i < MAX_FRUITS; i++) {
        tree[i] = i + 1;  // Fruits are numbered 1 to MAX_FRUITS
    }
}

// Picker thread function
void* picker(void* arg) {
    int id = *((int*)arg);  // Picker ID

    while (1) {
        int fruit = -1;

        // ------------------ STEP 1: Pick a fruit from the tree ------------------
        pthread_mutex_lock(&tree_mutex);  // Lock the tree before picking
        if (tree_size > 0) {
            tree_size = tree_size - 1;       // Decrease the fruit count
            fruit = tree[tree_size];         // Take the last fruit
            printf("Picker %d picked fruit %d\n", id, fruit);
        }
        pthread_mutex_unlock(&tree_mutex);  // Unlock the tree

        if (fruit == -1) {
            break;  // No more fruits to pick
        }

        // ------------------ STEP 2: Add fruit to crate ------------------
        pthread_mutex_lock(&crate_mutex);  // Lock the crate

        // Wait if the crate is full and hasn't been emptied yet
        while (crate_count == CRATE_SIZE) {
            pthread_cond_wait(&crate_emptied, &crate_mutex);  // Wait for loader to empty
        }

        crate[crate_count] = fruit;         // Place fruit in crate
        crate_count = crate_count + 1;      // Update crate count
        printf("Picker %d placed fruit %d in crate slot %d\n", id, fruit, crate_count);

        // If crate is full, notify the loader
        if (crate_count == CRATE_SIZE) {
            printf("Picker %d: Crate is full. Notifying loader...\n", id);
            pthread_cond_signal(&crate_full);  // Wake up loader
        }

        pthread_mutex_unlock(&crate_mutex);  // Unlock the crate

        usleep(100000);  // Simulate a delay (100 ms)
    }

    printf("Picker %d is done.\n", id);
    pthread_exit(NULL);
    return NULL;
}

// Loader thread function
void* loader(void* arg) {
    (void)arg; // Add this line at the top of the function

    while (1) {
        pthread_mutex_lock(&crate_mutex);

        // Wait for crate to be full OR all fruits are picked
        while (crate_count < CRATE_SIZE) {
            // Check if picking is done
            pthread_mutex_lock(&tree_mutex);
            int no_more_fruits = (tree_size == 0);
            pthread_mutex_unlock(&tree_mutex);

            if (no_more_fruits) {
                break;  // No need to wait anymore
            }

            pthread_cond_wait(&crate_full, &crate_mutex);  // Wait for crate to be full
        }

        // If there's something in the crate, move it to truck
        if (crate_count > 0) {
            printf("Loader: Moving crate to truck [ ");
            for (int i = 0; i < crate_count; i++) {
                printf("%d ", crate[i]);
                crate[i] = -1;  // Empty the crate slot
            }
            printf("]\n");

            crate_count = 0;  // Reset crate count
            pthread_cond_broadcast(&crate_emptied);  // Wake all pickers waiting for empty crate
        }

        pthread_mutex_unlock(&crate_mutex);

        // Check if everything is done
        pthread_mutex_lock(&tree_mutex);
        int done = (tree_size == 0);
        pthread_mutex_unlock(&tree_mutex);

        if (done && crate_count == 0) {
            break;  // Exit loader if all fruits picked and crate is empty
        }

        usleep(150000);  // Simulate loading delay (150 ms)
    }

    printf("Loader: All crates delivered. Exiting.\n");
    pthread_exit(NULL);
    return NULL;
}

// ------------------ MAIN FUNCTION ------------------
int main() {
    int picker_ids[PICKER_COUNT];  // Array to store picker IDs

    init_tree();  // Fill the tree with fruits

    // Initialize crate with empty slots
    for (int i = 0; i < CRATE_SIZE; i++) {
        crate[i] = -1;
    }

    // Create the loader thread
    pthread_create(&loader_thread, NULL, loader, NULL);

    // Create the picker threads
    for (int i = 0; i < PICKER_COUNT; i++) {
        picker_ids[i] = i + 1;  // Assign ID starting from 1
        pthread_create(&pickers[i], NULL, picker, &picker_ids[i]);
    }

    // Wait for all picker threads to finish
    for (int i = 0; i < PICKER_COUNT; i++) {
        pthread_join(pickers[i], NULL);
    }

    // Wake up loader in case it's waiting and pickers are done
    pthread_cond_signal(&crate_full);

    // Wait for the loader to finish
    pthread_join(loader_thread, NULL);

    // Clean up
    pthread_mutex_destroy(&tree_mutex);
    pthread_mutex_destroy(&crate_mutex);
    pthread_cond_destroy(&crate_full);
    pthread_cond_destroy(&crate_emptied);

    printf("Main: All work done. Simulation complete.\n");
    return 0;
}
