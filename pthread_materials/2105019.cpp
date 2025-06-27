#include <chrono>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <random>
#include <unistd.h>
#include <vector>
#include <semaphore.h>

using namespace std;

#define MAX_WRITING_TIME 5
#define WALKING_TO_PRINTER 10
#define SLEEP_MULTIPLIER 1000 
#define NUM_STATIONS 4

int N, M, x, y;
int operations_completed = 0;

pthread_mutex_t output_lock;
pthread_mutex_t log_mutex;
sem_t stations[NUM_STATIONS];
sem_t logbook_write;
pthread_mutex_t group_locks[100];
int group_progress[100];
bool group_finished[100];

pthread_mutex_t reader_count_mutex;
int reader_count = 0;

auto start_time = std::chrono::high_resolution_clock::now();
long long get_time()
{
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
}

int get_random_number()
{
    std::random_device rd;
    std::mt19937 generator(rd());
    double lambda = 1.5;
    std::poisson_distribution<int> poissonDist(lambda);
    int result = poissonDist(generator);
    return result > 0 ? result : 1;
}

class Operative
{
public:
    int id;
    int group_id;
    bool is_leader;
    int writing_time;
    Operative(int _id) : id(_id)
    {
        group_id = (id - 1) / M;
        is_leader = (id % M == 0);
        writing_time = get_random_number() % MAX_WRITING_TIME + 1;
    }
};

vector<Operative> operatives;
bool started[10005] = {false};
bool simulation_running = true;

void write_output(const std::string &output)
{
    pthread_mutex_lock(&output_lock);
    std::cout << output;
    fflush(stdout);
    pthread_mutex_unlock(&output_lock);
}

void *logbook_reader(void *arg)
{
    std::string name = *(std::string *)arg;
    while (simulation_running)
    {
        usleep((rand() % 200 + 300) * 1000); 

        pthread_mutex_lock(&reader_count_mutex);
        if (++reader_count == 1)
            sem_wait(&logbook_write);
        pthread_mutex_unlock(&reader_count_mutex);
        write_output(name + " began reviewing logbook at time " + std::to_string(get_time()) + ". Operations completed = " + std::to_string(operations_completed) + "\n");
        usleep((rand() % 20 + 10) * 1000); 
        pthread_mutex_lock(&reader_count_mutex);
        if (--reader_count == 0)
            sem_post(&logbook_write);
        pthread_mutex_unlock(&reader_count_mutex);
    }
    return NULL;
}

void *operative_activity(void *arg)
{
    Operative *op = (Operative *)arg;
    usleep((rand() % 5 + 1) * 1000); 
    int station_id = op->id % NUM_STATIONS;
    sem_wait(&stations[station_id]);
    write_output("Operative " + std::to_string(op->id) + " arrived at typewriting station at time " + std::to_string(get_time()) + "\n");
    usleep(x * SLEEP_MULTIPLIER); 
    write_output("Operative " + std::to_string(op->id) + " has completed document recreation at time " + std::to_string(get_time()) + "\n");
    sem_post(&stations[station_id]);
    pthread_mutex_lock(&group_locks[op->group_id]);
    group_progress[op->group_id]++;
    if (group_progress[op->group_id] == M)
    {
        group_finished[op->group_id] = true;
    }
    pthread_mutex_unlock(&group_locks[op->group_id]);
    if (op->is_leader)
    {
        while (!group_finished[op->group_id])
            usleep(100);
        usleep(y * SLEEP_MULTIPLIER); 
        sem_wait(&logbook_write);
        pthread_mutex_lock(&log_mutex);
        operations_completed++;
        write_output("Unit" + std::to_string(op->group_id) + " has completed document recreation phase at time " + std::to_string(get_time()) + "\n");
        pthread_mutex_unlock(&log_mutex);
        sem_post(&logbook_write);
    }
    return NULL;
}

void *timeout_watcher(void *)
{
    usleep(15000000); 
    std::cerr << "Simulation timed out\n";
    exit(1);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cout << "Usage: ./a.out <input_file> <output_file>\n";
        return 0;
    }

    std::ifstream inputFile(argv[1]);
    std::streambuf *cinBuffer = std::cin.rdbuf();
    std::cin.rdbuf(inputFile.rdbuf());

    std::ofstream outputFile(argv[2]);
    std::streambuf *coutBuffer = std::cout.rdbuf();
    std::cout.rdbuf(outputFile.rdbuf());

    std::cin >> N >> M >> x >> y;

    pthread_mutex_init(&output_lock, NULL);
    pthread_mutex_init(&log_mutex, NULL);
    pthread_mutex_init(&reader_count_mutex, NULL);
    sem_init(&logbook_write, 0, 1);

    for (int i = 0; i < NUM_STATIONS; i++)
        sem_init(&stations[i], 0, 1);
    for (int i = 0; i < 100; i++)
        pthread_mutex_init(&group_locks[i], NULL);

    for (int i = 1; i <= N; i++)
        operatives.emplace_back(i);

    std::string r1 = "Intelligence Staff 1", r2 = "Intelligence Staff 2";
    pthread_t readers[2];
    pthread_create(&readers[0], NULL, logbook_reader, &r1);
    pthread_create(&readers[1], NULL, logbook_reader, &r2);

    pthread_t timeout_thread;
    pthread_create(&timeout_thread, NULL, timeout_watcher, NULL);

    pthread_t operative_threads[N];
    int remaining = N;
    while (remaining > 0)
    {
        int idx = get_random_number() % N;
        if (!started[idx])
        {
            started[idx] = true;
            pthread_create(&operative_threads[idx], NULL, operative_activity, &operatives[idx]);
            remaining--;
            usleep(5000);          
            if (get_time() > 1000) 
            {
                break;
            }
        }
    }

    for (int i = 0; i < N; i++)
    {
        if (!started[i])
        {
            pthread_create(&operative_threads[i], NULL, operative_activity, &operatives[i]);
        }
    }

    for (int i = 0; i < N; i++)
        pthread_join(operative_threads[i], NULL);

    sleep(1);
    simulation_running = false;
    pthread_cancel(readers[0]);
    pthread_cancel(readers[1]);

    std::cin.rdbuf(cinBuffer);
    std::cout.rdbuf(coutBuffer);

    return 0;
}