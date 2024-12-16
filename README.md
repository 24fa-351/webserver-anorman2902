# 351-webserver

This project implements a multithreaded HTTP/1.1 web server in C. It supports three key features: serving static files, displaying server statistics, and performing basic arithmetic calculations.

---

## Features

1. **Static File Serving (`/static`)**:
   - Serves binary files from the `/static` directory.
   - Example: `GET /static/images/rex.png`.

2. **Server Statistics (`/stats`)**:
   - Displays the number of requests received, total bytes received, and total bytes sent in an HTML response.

3. **Basic Calculations (`/calc`)**:
   - Sums two query parameters (`a` and `b`) and returns the result as plain text or HTML.
   - Example: `GET /calc?a=5&b=10` returns `Result: 15`.

---

## Usage

### 1. Compile
Use the provided Makefile to compile the server:
```bash
make
