# QuizMaster Pro — API-Powered Terminal Quiz Engine in C

A modular, real-time command-line quiz application written in **C** that fetches live trivia questions from the **[Open Trivia Database (OpenTDB) API](https://opentdb.com/)**, parses JSON payloads in memory, decodes HTML entities, enforces per-question timers, and persists player history and sorted leaderboards to disk.

## Key Features
- **Live REST API Integration:** Dynamically constructs query URLs and executes HTTP GET requests to the OpenTDB API across 5 categories and 3 difficulty levels.
- **Custom Lightweight JSON Parser & HTML Decoder:** Extracts nested question objects, options, and metadata using pointer arithmetic (`strstr`, `strchr`, `strncpy`) and decodes HTML entities (`&quot;`, `&#039;`, `&amp;`) in-place using a two-pointer algorithm.
- **Unbiased Option Randomization:** Implements the **Fisher-Yates Shuffle Algorithm** while dynamically tracking the correct option index.
- **Per-Question Countdown Timer:** Measures response latency using `<time.h>` (`difftime`) and enforces a strict 15-second limit per question.
- **Offline Backup Mode:** Automatically falls back to built-in C programming questions if the internet connection is unavailable.
- **Crash-Proof Terminal Input:** Replaces unsafe `scanf` calls with buffered `fgets` + `sscanf` validation to prevent `stdin` buffer infinite loops.
- **Persistent Storage & Tie-Breaking Leaderboard:** Stores session logs in CSV format (`quiz_history.txt`) and ranks the Top 10 players by highest score first, using fastest completion time as the tie-breaker.

## Compile & Run
```bash
gcc -Wall -Wextra Quiz_master.c -o Quiz_master
.\Quiz_master.exe