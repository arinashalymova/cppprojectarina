#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <ctime>
#include <algorithm>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

struct Task {
    int id;
    std::string title;
    std::string description;
    std::string dueDate;
    std::string category;
    bool completed;
};

std::vector<Task> tasks;
int nextTaskId = 1;

std::string determineCategory(const std::string& title, const std::string& description) {
    std::map<std::string, std::vector<std::string>> categoryKeywords = {
        {"Работа", {"работа", "проект", "документ", "отчет", "встреча", "звонок"}},
        {"Учеба", {"учеба", "задание", "курс", "книга", "читать", "учить"}},
        {"Покупки", {"купить", "магазин", "список", "продукты", "заказать"}},
        {"Здоровье", {"спорт", "тренировка", "врач", "лекарство", "здоровье"}},
        {"Дом", {"уборка", "ремонт", "готовка", "стирка", "дом", "квартира"}}
    };
    
    std::string textToCheck = title + " " + description;
    std::transform(textToCheck.begin(), textToCheck.end(), textToCheck.begin(), ::tolower);
    
    for (const auto& category : categoryKeywords) {
        for (const auto& keyword : category.second) {
            if (textToCheck.find(keyword) != std::string::npos) {
                return category.first;
            }
        }
    }
    
    return "Другое";
}

void loadTasks() {
    std::ifstream file("tasks.txt");
    if (!file.is_open()) {
        return; 
    }
    
    tasks.clear();
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        Task task;
        std::string completedStr;
        
        if (iss >> task.id >> std::ws && std::getline(iss, task.title, '|') &&
            std::getline(iss, task.description, '|') &&
            std::getline(iss, task.dueDate, '|') &&
            std::getline(iss, task.category, '|') &&
            std::getline(iss, completedStr)) {
            
            task.completed = (completedStr == "1");
            tasks.push_back(task);
            
            if (task.id >= nextTaskId) {
                nextTaskId = task.id + 1;
            }
        }
    }
    
    file.close();
}

void saveTasks() {
    std::ofstream file("tasks.txt");
    if (!file.is_open()) {
        std::cerr << "Не удалось открыть файл для записи\n";
        return;
    }
    
    for (const auto& task : tasks) {
        file << task.id << " " << task.title << "|"
             << task.description << "|"
             << task.dueDate << "|"
             << task.category << "|"
             << (task.completed ? "1" : "0") << "\n";
    }
    
    file.close();
}

bool isTaskDueSoon(const std::string& dueDate) {
    if (dueDate.empty()) return false;
    
    std::time_t now = std::time(nullptr);
    std::tm* currentTime = std::localtime(&now);
    int currentYear = currentTime->tm_year + 1900;
    int currentMonth = currentTime->tm_mon + 1;
    int currentDay = currentTime->tm_mday;
    
    if (dueDate.length() >= 10) {
        try {
            int dueYear = std::stoi(dueDate.substr(0, 4));
            int dueMonth = std::stoi(dueDate.substr(5, 2));
            int dueDay = std::stoi(dueDate.substr(8, 2));
            
            if (dueYear == currentYear) {
                if (dueMonth == currentMonth) {
                    int dayDiff = dueDay - currentDay;
                    return (dayDiff >= 0 && dayDiff <= 3);
                } else if (dueMonth == currentMonth + 1 && dueDay <= 3 && currentDay >= 28) {
                    return true;
                }
            }
        } catch (...) {
            return false;
        }
    }
    
    return false;
}

std::string urlDecode(const std::string& encoded) {
    std::string result;
    for (size_t i = 0; i < encoded.length(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.length()) {
            int value;
            std::istringstream iss(encoded.substr(i + 1, 2));
            iss >> std::hex >> value;
            result += static_cast<char>(value);
            i += 2;
        } else if (encoded[i] == '+') {
            result += ' ';
        } else {
            result += encoded[i];
        }
    }
    return result;
}

std::map<std::string, std::string> parsePostData(const std::string& data) {
    std::map<std::string, std::string> result;
    std::istringstream iss(data);
    std::string pair;
    
    while (std::getline(iss, pair, '&')) {
        size_t pos = pair.find('=');
        if (pos != std::string::npos) {
            std::string key = urlDecode(pair.substr(0, pos));
            std::string value = urlDecode(pair.substr(pos + 1));
            result[key] = value;
        }
    }
    
    return result;
}

std::string generateHTML() {
    std::stringstream html;
    
    html << "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
         << "<!DOCTYPE html>\n"
         << "<html lang=\"ru\">\n"
         << "<head>\n"
         << "    <meta charset=\"UTF-8\">\n"
         << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
         << "    <title>Ежедневник</title>\n"
         << "    <style>\n"
         << "        body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f5f5f5; }\n"
         << "        h1 { color: #333; text-align: center; }\n"
         << "        .container { max-width: 800px; margin: 0 auto; background-color: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }\n"
         << "        .task { border: 1px solid #ddd; margin: 10px 0; padding: 10px; border-radius: 4px; }\n"
         << "        .task.completed { background-color: #e8f5e9; }\n"
         << "        .task.due-soon { border-left: 4px solid #f44336; }\n"
         << "        .task-header { display: flex; justify-content: space-between; margin-bottom: 8px; }\n"
         << "        .task-title { font-weight: bold; margin-right: 10px; }\n"
         << "        .task-category { color: #555; font-size: 0.9em; }\n"
         << "        .task-description { margin-bottom: 10px; color: #666; }\n"
         << "        .task-footer { display: flex; justify-content: space-between; font-size: 0.85em; color: #777; }\n"
         << "        form { margin-top: 20px; }\n"
         << "        label { display: block; margin-top: 10px; }\n"
         << "        input[type=\"text\"], textarea { width: 100%; padding: 8px; margin-top: 5px; border: 1px solid #ddd; border-radius: 4px; }\n"
         << "        input[type=\"date\"] { padding: 8px; margin-top: 5px; border: 1px solid #ddd; border-radius: 4px; }\n"
         << "        button { background-color: #4caf50; color: white; padding: 10px 15px; border: none; border-radius: 4px; cursor: pointer; margin-top: 10px; }\n"
         << "        button:hover { background-color: #45a049; }\n"
         << "        .btn-delete { background-color: #f44336; }\n"
         << "        .btn-delete:hover { background-color: #d32f2f; }\n"
         << "        .btn-complete { background-color: #2196f3; }\n"
         << "        .btn-complete:hover { background-color: #1976d2; }\n"
         << "        .actions { display: flex; gap: 5px; }\n"
         << "        .reminder { background-color: #fff3e0; border-left: 4px solid #ff9800; padding: 10px; margin-bottom: 20px; }\n"
         << "    </style>\n"
         << "</head>\n"
         << "<body>\n"
         << "    <div class=\"container\">\n"
         << "        <h1>Ежедневник</h1>\n";
    
    std::vector<Task> dueSoonTasks;
    for (const auto& task : tasks) {
        if (!task.completed && isTaskDueSoon(task.dueDate)) {
            dueSoonTasks.push_back(task);
        }
    }
    
    if (!dueSoonTasks.empty()) {
        html << "        <div class=\"reminder\">\n"
             << "            <strong>Напоминание!</strong> У вас есть задачи, которые нужно выполнить в ближайшее время:\n"
             << "            <ul>\n";
        for (const auto& task : dueSoonTasks) {
            html << "                <li>" << task.title << " (срок: " << task.dueDate << ")</li>\n";
        }
        html << "            </ul>\n"
             << "        </div>\n";
    }
    
    html << "        <form action=\"/add\" method=\"post\">\n"
         << "            <h2>Добавить новую задачу</h2>\n"
         << "            <label for=\"title\">Название:</label>\n"
         << "            <input type=\"text\" id=\"title\" name=\"title\" required>\n"
         << "            <label for=\"description\">Описание:</label>\n"
         << "            <textarea id=\"description\" name=\"description\" rows=\"3\"></textarea>\n"
         << "            <label for=\"dueDate\">Срок выполнения:</label>\n"
         << "            <input type=\"date\" id=\"dueDate\" name=\"dueDate\">\n"
         << "            <button type=\"submit\">Добавить задачу</button>\n"
         << "        </form>\n"
         << "        <h2>Список задач</h2>\n";
    
    if (tasks.empty()) {
        html << "        <p>Нет задач. Добавьте новую задачу выше.</p>\n";
    } else {
        std::vector<Task> sortedTasks = tasks;
        std::stable_sort(sortedTasks.begin(), sortedTasks.end(), [](const Task& a, const Task& b) {
            if (a.completed == b.completed) return false;
            return !a.completed; 
        });
        
        for (const auto& task : sortedTasks) {
            std::string taskClass = task.completed ? "task completed" : "task";
            if (!task.completed && isTaskDueSoon(task.dueDate)) {
                taskClass += " due-soon";
            }
            
            html << "        <div class=\"" << taskClass << "\">\n"
                 << "            <div class=\"task-header\">\n"
                 << "                <div class=\"task-title\">" << task.title << "</div>\n"
                 << "                <div class=\"task-category\">Категория: " << task.category << "</div>\n"
                 << "            </div>\n"
                 << "            <div class=\"task-description\">" << task.description << "</div>\n"
                 << "            <div class=\"task-footer\">\n"
                 << "                <div>Срок: " << (task.dueDate.empty() ? "Не указан" : task.dueDate) << "</div>\n"
                 << "                <div class=\"actions\">\n"
                 << "                    <form action=\"/complete\" method=\"post\" style=\"margin: 0;\">\n"
                 << "                        <input type=\"hidden\" name=\"id\" value=\"" << task.id << "\">\n";
                 
            if (!task.completed) {
                html << "                        <button type=\"submit\" class=\"btn-complete\">Выполнено</button>\n";
            } else {
                html << "                        <button type=\"submit\" class=\"btn-complete\">Отменить выполнение</button>\n";
            }
            
            html << "                    </form>\n"
                 << "                    <form action=\"/delete\" method=\"post\" style=\"margin: 0;\">\n"
                 << "                        <input type=\"hidden\" name=\"id\" value=\"" << task.id << "\">\n"
                 << "                        <button type=\"submit\" class=\"btn-delete\">Удалить</button>\n"
                 << "                    </form>\n"
                 << "                </div>\n"
                 << "            </div>\n"
                 << "        </div>\n";
        }
    }
    
    html << "    </div>\n"
         << "</body>\n"
         << "</html>\n";
    
    return html.str();
}

std::string handleRequest(const std::string& request) {
    std::string requestLine = request.substr(0, request.find("\n"));
    std::string method = requestLine.substr(0, requestLine.find(" "));
    std::string path = requestLine.substr(requestLine.find(" ") + 1);
    path = path.substr(0, path.find(" "));
    
    if (method == "GET") {
        return generateHTML();
    } else if (method == "POST") {
        size_t pos = request.find("\r\n\r\n");
        if (pos != std::string::npos) {
            std::string postBody = request.substr(pos + 4);
            std::map<std::string, std::string> postData = parsePostData(postBody);
            
            if (path == "/add") {
                Task newTask;
                newTask.id = nextTaskId++;
                newTask.title = postData["title"];
                newTask.description = postData["description"];
                newTask.dueDate = postData["dueDate"];
                newTask.category = determineCategory(newTask.title, newTask.description);
                newTask.completed = false;
                
                tasks.push_back(newTask);
                saveTasks();
            } else if (path == "/delete") {
                int id = std::stoi(postData["id"]);
                auto it = std::find_if(tasks.begin(), tasks.end(), 
                                    [id](const Task& t) { return t.id == id; });
                if (it != tasks.end()) {
                    tasks.erase(it);
                    saveTasks();
                }
            } else if (path == "/complete") {
                int id = std::stoi(postData["id"]);
                auto it = std::find_if(tasks.begin(), tasks.end(), 
                                    [id](const Task& t) { return t.id == id; });
                if (it != tasks.end()) {
                    it->completed = !it->completed;
                    saveTasks();
                }
            }
        }
        
        return "HTTP/1.1 303 See Other\r\nLocation: /\r\n\r\n";
    }
    
    return "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nPage not found";
}

int main() {
    loadTasks();
    
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Ошибка при создании сокета\n";
        return 1;
    }
    
    int option = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(8080);
    
    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "Ошибка при привязке сокета к адресу\n";
        close(serverSocket);
        return 1;
    }
    
    if (listen(serverSocket, 5) < 0) {
        std::cerr << "Ошибка при прослушивании сокета\n";
        close(serverSocket);
        return 1;
    }
    
    std::cout << "Сервер запущен на http://localhost:8080\n";
    std::cout << "Для доступа к ежедневнику откройте браузер и перейдите по адресу: http://localhost:8080\n";
    std::cout << "Для остановки сервера нажмите Ctrl+C\n";
    
    while (true) {
        struct sockaddr_in clientAddress;
        socklen_t clientLength = sizeof(clientAddress);
        
        int clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddress, &clientLength);
        if (clientSocket < 0) {
            std::cerr << "Ошибка при принятии соединения\n";
            continue;
        }
        
        char buffer[4096] = {0};
        read(clientSocket, buffer, 4096);
        
        std::string response = handleRequest(buffer);
        write(clientSocket, response.c_str(), response.size());
        
        close(clientSocket);
    }
    
    close(serverSocket);
    
    return 0;
}
