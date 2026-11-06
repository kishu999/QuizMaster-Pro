#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_QUESTION_LEN 256
#define MAX_OPTION_LEN 128
#define MAX_CATEGORY_LEN 64
#define MAX_DIFFICULTY_LEN 16
#define MAX_NAME_LEN 50
#define MAX_DATE_LEN 32
#define NUM_OPTIONS 4
#define TIME_LIMIT 15.0

#define COLOR_RESET  "\033[0m"
#define COLOR_RED    "\033[1;31m"
#define COLOR_GREEN  "\033[1;32m"
#define COLOR_YELLOW "\033[1;33m"
#define COLOR_CYAN   "\033[1;36m"

typedef struct {
    char question_text[MAX_QUESTION_LEN];
    char options[NUM_OPTIONS][MAX_OPTION_LEN];
    int correct_option;                     
    char category[MAX_CATEGORY_LEN];
    char difficulty[MAX_DIFFICULTY_LEN];
} Question;

typedef struct {
    int num_questions;
    int category_id;
    char difficulty[MAX_DIFFICULTY_LEN];
} QuizConfig;

typedef struct {
    char player_name[MAX_NAME_LEN];
    int score;
    int total_questions;
    double time_taken_sec;
    char date_str[MAX_DATE_LEN];
} PlayerSession;

void get_valid_string(char *dest, int max_len) {
    while (1) {
        if (fgets(dest, max_len, stdin) != NULL) {
            dest[strcspn(dest, "\n")] = '\0';
            if (strlen(dest) > 0) {
                break;
            } else {
                printf("Input cannot be empty! Please try again: ");
            }
        }
    }
}

int get_valid_int(int min, int max) {
    char buffer[100];
    int value;
    char extra;

    while (1) {
        if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
            int parsed = sscanf(buffer, "%d %c", &value, &extra);
            if (parsed == 1 && value >= min && value <= max) {
                return value;
            } else {
                printf("Invalid choice! Enter a number between %d and %d: ", min, max);
            }
        }
    }
}

void shuffle_options(Question *q) {
    char temp[MAX_OPTION_LEN];

    for (int i = NUM_OPTIONS - 1; i > 0; i--) {
        int j = rand() % (i + 1); 

        strcpy(temp, q->options[i]);
        strcpy(q->options[i], q->options[j]);
        strcpy(q->options[j], temp);

        if (q->correct_option == i) {
            q->correct_option = j;
        } else if (q->correct_option == j) {
            q->correct_option = i;
        }
    }
}

void configure_quiz(QuizConfig *config) {
    printf("\n--- SELECT CATEGORY ---\n");
    printf("1. General Knowledge\n");
    printf("2. PPS\n");
    printf("3. Science & Nature\n");
    printf("4. History\n");
    printf("5. Sports\n");
    printf("Choose a category (1-5): ");
    int cat_choice = get_valid_int(1, 5);

    int category_ids[5] = {9, 18, 17, 23, 21};
    config->category_id = category_ids[cat_choice - 1];

    printf("\n--- SELECT DIFFICULTY ---\n");
    printf("1. Easy\n");
    printf("2. Medium\n");
    printf("3. Hard\n");
    printf("Choose difficulty (1-3): ");
    int diff_choice = get_valid_int(1, 3);

    if (diff_choice == 1) {
        strcpy(config->difficulty, "easy");
    } else if (diff_choice == 2) {
        strcpy(config->difficulty, "medium");
    } else {
        strcpy(config->difficulty, "hard");
    }

    printf("\nHow many questions would you like? (3-10): ");
    config->num_questions = get_valid_int(3, 10);

    printf("\n[Config Saved: Category ID=%d | Difficulty=%s | Questions=%d]\n",
           config->category_id, config->difficulty, config->num_questions);
}

void build_api_url(const QuizConfig *config, char *url_buffer, int buffer_size) {
    snprintf(url_buffer, buffer_size,
             "https://opentdb.com/api.php?amount=%d&category=%d&difficulty=%s&type=multiple",
             config->num_questions, config->category_id, config->difficulty);
}

int fetch_api_response(const char *url, char *response_buffer, int buffer_size) {
    char command[512];
    snprintf(command, sizeof(command), "curl -s \"%s\"", url);

    FILE *pipe = _popen(command, "r");
    if (pipe == NULL) {
        return 0;
    }

    response_buffer[0] = '\0';
    char chunk[256];

    while (fgets(chunk, sizeof(chunk), pipe) != NULL) {
        if (strlen(response_buffer) + strlen(chunk) < (size_t)buffer_size - 1) {
            strcat(response_buffer, chunk);
        }
    }

    _pclose(pipe);

    if (strlen(response_buffer) == 0) {
        return 0;
    } else {
        return 1;
    }
}

void decode_html_entities(char *str) {
    int i = 0;
    int j = 0;

    while (str[i] != '\0') {
        if (strncmp(&str[i], "&quot;", 6) == 0) {
            str[j++] = '"';
            i += 6;
        } else if (strncmp(&str[i], "&#039;", 6) == 0) {
            str[j++] = '\'';
            i += 6;
        } else if (strncmp(&str[i], "&amp;", 5) == 0) {
            str[j++] = '&';
            i += 5;
        } else {
            str[j++] = str[i++];
        }
    }
    str[j] = '\0';
}

int extract_quoted_value(const char *json_ptr, const char *key, char *dest, int max_len) {
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "\"%s\":\"", key);
    
    char *start = strstr(json_ptr, search_key);
    if (start == NULL) return 0;
    
    start += strlen(search_key);
    
    char *end = strchr(start, '"');
    if (end == NULL) return 0;
    
    int len = (int)(end - start);
    if (len >= max_len) len = max_len - 1;
    
    strncpy(dest, start, len);
    dest[len] = '\0';
    
    decode_html_entities(dest);
    return 1;
}

int parse_questions_from_json(const char *json_response, Question *questions, const QuizConfig *config) {
    const char *ptr = json_response;
    
    for (int i = 0; i < config->num_questions; i++) {
        ptr = strstr(ptr, "\"type\":");
        if (ptr == NULL) {
            printf("[ERROR] Failed to parse question %d from JSON response.\n", i + 1);
            return 0;
        }
        
        if (!extract_quoted_value(ptr, "category", questions[i].category, MAX_CATEGORY_LEN)) return 0;
        if (!extract_quoted_value(ptr, "difficulty", questions[i].difficulty, MAX_DIFFICULTY_LEN)) return 0;
        if (!extract_quoted_value(ptr, "question", questions[i].question_text, MAX_QUESTION_LEN)) return 0;
        if (!extract_quoted_value(ptr, "correct_answer", questions[i].options[0], MAX_OPTION_LEN)) return 0;
        
        char *inc_ptr = strstr(ptr, "\"incorrect_answers\":[");
        if (inc_ptr == NULL) return 0;
        inc_ptr += strlen("\"incorrect_answers\":[");
        
        for (int j = 1; j < NUM_OPTIONS; j++) {
            char *q_start = strchr(inc_ptr, '"');
            if (q_start == NULL) return 0;
            q_start++;
            
            char *q_end = strchr(q_start, '"');
            if (q_end == NULL) return 0;
            
            int len = (int)(q_end - q_start);
            if (len >= MAX_OPTION_LEN) len = MAX_OPTION_LEN - 1;
            
            strncpy(questions[i].options[j], q_start, len);
            questions[i].options[j][len] = '\0';
            
            decode_html_entities(questions[i].options[j]);
            
            inc_ptr = q_end + 1;
        }
        
        questions[i].correct_option = 0;
        ptr = inc_ptr;
    }
    
    return 1;
}

int load_fallback_questions(Question *questions) {
    Question backup[3] = {
        {
            "Which C function is used to dynamically allocate memory?",
            {"malloc()", "alloc()", "mem_create()", "new()"},
            0, "Science: Computers (Offline)", "easy"
        },
        {
            "What does HTTP stand for?",
            {"HyperText Transfer Protocol", "HighText Transfer Process", "Hyperlink Text Protocol", "Home Tool Transfer Protocol"},
            0, "Science: Computers (Offline)", "easy"
        },
        {
            "Which operator is used to access structure members through a pointer?",
            {"->", ".", "*", "&"},
            0, "Science: Computers (Offline)", "medium"
        }
    };

    for (int i = 0; i < 3; i++) {
        questions[i] = backup[i];
    }
    return 3;
}

void play_quiz(PlayerSession *player, Question *questions, int num_questions) {
    player->score = 0;
    player->total_questions = num_questions;
    player->time_taken_sec = 0.0;

    printf("\n=========================================\n");
    printf("   QUIZ STARTED! (Time Limit: 15s/Q)     \n");
    printf("=========================================\n");

    for (int i = 0; i < player->total_questions; i++) {
        shuffle_options(&questions[i]);

        printf("\nQ%d [%s | %s]:\n%s\n", i + 1, questions[i].category, questions[i].difficulty, questions[i].question_text);
        for (int j = 0; j < NUM_OPTIONS; j++) {
            printf("  %d. %s\n", j + 1, questions[i].options[j]);
        }

        printf("Your answer (1-4): ");
        time_t start_time = time(NULL);
        int user_ans = get_valid_int(1, 4);
        time_t end_time = time(NULL);

        double time_spent = difftime(end_time, start_time);
        player->time_taken_sec += time_spent;

       if (time_spent > TIME_LIMIT) {
            printf("%sTIME UP! You took %.0f seconds (Limit: 15s).%s\n", COLOR_RED, time_spent, COLOR_RESET);
            printf("Correct Answer was: %s\n", questions[i].options[questions[i].correct_option]);
        } else if ((user_ans - 1) == questions[i].correct_option) {
            printf("%sCORRECT!! (Answered in %.0f seconds)%s\n", COLOR_GREEN, time_spent, COLOR_RESET);
            player->score++;
        } else {
            printf("%sWRONG! Correct Answer was: %s%s\n", COLOR_RED, questions[i].options[questions[i].correct_option], COLOR_RESET);
        }
    }

    printf("\n=========================================\n");
    printf("            FINAL SCOREBOARD             \n");
    printf("=========================================\n");
    printf("Player Name    : %s\n", player->player_name);
    printf("Final Score    : %d / %d\n", player->score, player->total_questions);
    printf("Total Time     : %.0f seconds\n", player->time_taken_sec);
    printf("=========================================\n");
}

#define HISTORY_FILE "quiz_history.txt"

void save_quiz_session(PlayerSession *player) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(player->date_str, MAX_DATE_LEN, "%Y-%m-%d %H:%M", t);

    
    FILE *fp = fopen(HISTORY_FILE, "a");
    if (fp == NULL) {
        printf("[ERROR] Could not open %s to save score!\n", HISTORY_FILE);
        return;
    }

    fprintf(fp, "%s,%d,%d,%.0f,%s\n",
            player->player_name, player->score, player->total_questions,
            player->time_taken_sec, player->date_str);

             fclose(fp);
    printf("[Session saved to %s!]\n", HISTORY_FILE);
}

void view_quiz_history(void) {
    FILE *fp = fopen(HISTORY_FILE, "r");
    
    if (fp == NULL) {
        printf("\n[No quiz history found yet! Play a quiz first.]\n");
        return;
    }

    printf("\n====================================================================\n");
    printf("                           QUIZ HISTORY                             \n");
    printf("====================================================================\n");
    printf("%-18s | %-9s | %-8s | %-16s\n", "Player Name", "Score", "Time(s)", "Date & Time");
    printf("--------------------------------------------------------------------\n");

    PlayerSession s;
    while (fscanf(fp, "%49[^,],%d,%d,%lf,%31[^\n]\n",
                  s.player_name, &s.score, &s.total_questions,
                  &s.time_taken_sec, s.date_str) == 5) {
        printf("%-18s | %2d / %-4d | %6.0fs  | %-16s\n",
               s.player_name, s.score, s.total_questions,
               s.time_taken_sec, s.date_str);
    }
    printf("====================================================================\n");

    fclose(fp);
}

#define MAX_RECORDS 100

void view_leaderboard(void) {
    FILE *fp = fopen(HISTORY_FILE, "r");
    if (fp == NULL) {
        printf("\n[No leaderboard records yet! Play a quiz first.]\n");
        return;
    }

    PlayerSession records[MAX_RECORDS];
    int count = 0;

    while (count < MAX_RECORDS &&
           fscanf(fp, "%49[^,],%d,%d,%lf,%31[^\n]\n",
                  records[count].player_name, &records[count].score,
                  &records[count].total_questions, &records[count].time_taken_sec,
                  records[count].date_str) == 5) {
        count++;
    }
    fclose(fp);

    if (count == 0) {
        printf("\n[Leaderboard is empty!]\n");
        return;
    }

    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            int should_swap = 0;

            if (records[j + 1].score > records[j].score) {
                should_swap = 1;
            } else if (records[j + 1].score == records[j].score &&
                       records[j + 1].time_taken_sec < records[j].time_taken_sec) {
                should_swap = 1;
            }

            if (should_swap) {
                PlayerSession temp = records[j];
                records[j] = records[j+1];
                records[j + 1] =temp;
            }
        }
    }

    printf("\n==========================================================================\n");
    printf("                         TOP 10 LEADERBOARD                               \n");
    printf("==========================================================================\n");
    printf("%-5s | %-18s | %-9s | %-8s | %-16s\n", "Rank", "Player Name", "Score", "Time(s)", "Date");
    printf("--------------------------------------------------------------------------\n");

    int limit = (count < 10) ? count : 10;
    for (int i = 0; i < limit; i++) {
        printf("#%-4d | %-18s | %2d / %-4d | %6.0fs  | %-16s\n",
               i + 1, records[i].player_name, records[i].score,
               records[i].total_questions, records[i].time_taken_sec,
               records[i].date_str);
    }
    printf("==========================================================================\n");
}

int main() {
    PlayerSession player;
    QuizConfig config;
    int choice;
    srand((unsigned int)time(NULL));

    printf("%s=========================================\n", COLOR_CYAN);
    printf("        WELCOME TO QUIZMASTER PRO        \n");
    printf("=========================================%s\n", COLOR_RESET);
    printf("Enter your player name: ");
    get_valid_string(player.player_name, MAX_NAME_LEN);

    while (1) {
        printf("\n%s--- MAIN MENU (Player: %s) ---%s\n", COLOR_YELLOW, player.player_name, COLOR_RESET);
        printf("1. Start New Quiz\n");
        printf("2. View Leaderboard\n");
        printf("3. View Quiz History\n");
        printf("4. Exit\n");
        printf("Select an option (1-4): ");

        choice = get_valid_int(1, 4);

        if (choice == 1) {
            configure_quiz(&config);
            char api_url[256];
            build_api_url(&config, api_url, sizeof(api_url));

            char json_response[8192];
            printf("%sFetching live questions from OpenTDB...%s\n", COLOR_CYAN, COLOR_RESET);

            if (fetch_api_response(api_url, json_response, sizeof(json_response))) {
                Question *questions = malloc(config.num_questions * sizeof(Question));
                if (questions == NULL) {
                    printf("[ERROR] Memory allocation failed!\n");
                    continue;
                }

                if (parse_questions_from_json(json_response, questions, &config)) {
                    play_quiz(&player, questions, config.num_questions);
                    save_quiz_session(&player);
                } else {
                    printf("%s[WARNING] API response error! Switching to Offline Backup Mode...%s\n", COLOR_YELLOW, COLOR_RESET);
                    Question backup_q[3];
                    int count = load_fallback_questions(backup_q);
                    play_quiz(&player, backup_q, count);
                    save_quiz_session(&player);
                }

                free(questions);
            } else {
                printf("%s\n[WARNING] No internet connection! Switching to Offline Backup Mode...%s\n", COLOR_YELLOW, COLOR_RESET);
                Question backup_q[3];
                int count = load_fallback_questions(backup_q);
                play_quiz(&player, backup_q, count);
                save_quiz_session(&player);
            }
        } else if (choice == 2) {
            view_leaderboard();
        } else if (choice == 3) {
            view_quiz_history();
        } else if (choice == 4) {
            printf("\n%sGoodbye, %s! Thanks for playing QuizMaster Pro.%s\n", COLOR_CYAN, player.player_name, COLOR_RESET);
            break;
        }
    }

    return 0;
}
