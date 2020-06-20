#include <iostream>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <SFML/Network.hpp>
#include <windows.h>
#include <Dwmapi.h>
#include <chrono>
#include <ctime>
#include <vector>
#include <fstream>
#include <thread>
#include <sstream>

using namespace std::literals::chrono_literals;

int pokemon_width;
int pokemon_height;

int mouseX, mouseY;

void hideConsole() {
    ShowWindow(GetConsoleWindow(), SW_HIDE);
}
void showConsole() {
    ShowWindow(GetConsoleWindow(), SW_SHOW);
}
bool isConsoleVisible() {
    return IsWindowVisible(GetConsoleWindow()) != FALSE;
}
void makeWindowTransparent(sf::RenderWindow& window) { 
    HWND hwnd = window.getSystemHandle();
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED); 
}
void makeWindowOnTop(sf::RenderWindow& window) { 
    HWND hwnd = window.getSystemHandle();
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}
void makeWindowOpaque(sf::RenderWindow& window) {
    HWND hwnd = window.getSystemHandle();
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
    RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
}
void setWindowAlpha(sf::RenderWindow& window, sf::Uint8 alpha = 255) {
    SetLayeredWindowAttributes(window.getSystemHandle(), 0, alpha, LWA_ALPHA);
}

// Разметка одного диапазона значений на другой
double map(double value, double istart, double iend, double ostart, double oend) {
    return (value - istart) * (oend - ostart) / (iend - istart) + ostart;
}

bool programIsQuitting = false;
void closeWindow(sf::RenderWindow& window) {
    programIsQuitting = true;
    window.close();
}

tm getCurrentTime() {
    time_t now = time(nullptr);
    tm newtime;
    localtime_s(&newtime, &now);

    return newtime;
}

// Структура, хранящая часы, минуты и секунды
struct Time {
    short hour;
    short min;
    short sec;

    // bool isNegative = false;
    
    Time() {
        hour = 0;
        min = 0;
        sec = 0;
    }
    Time(int Second) {
        sec = Second;
        min = sec / 60;
        hour = min / 60;

        sec %= 60;
        min %= 60;
        hour %= 24;
    }
    Time(int Hour, int Minute, int Second) {
        hour = Hour;
        min = Minute;
        sec = Second;
    }
    Time(tm& t) {
        hour = t.tm_hour;
        min = t.tm_min;
        sec = t.tm_sec;
    }

    int toSeconds() const {
        return sec + min * 60 + hour * 3600;
    }

    void fixTime() {
        int Second = toSeconds();
        sec = Second;
        min = sec / 60;
        hour = min / 60;

        sec %= 60;
        min %= 60;
        hour %= 24;
    }

    friend bool operator==(const Time& t1, const Time& t2);
    friend bool operator>=(const Time& t1, const Time& t2);
    friend bool operator<=(const Time& t1, const Time& t2);
    friend bool operator>(const Time& t1, const Time& t2);
    friend bool operator<(const Time& t1, const Time& t2);
    friend Time operator-(const Time& t1, const Time& t2);
    friend Time operator+(const Time& t1, const Time& t2);
    friend Time operator++(Time& t, int);
    friend Time operator--(Time& t, int);
};

bool operator==(const Time& t1, const Time& t2) {
    return t1.hour == t2.hour && t1.min == t2.min && t1.sec == t2.sec;
}

bool operator>(const Time& left, const Time& right) {
    if (left.hour > right.hour) return true;
    else if (left.hour == right.hour) {
        if (left.min > right.min) return true;
        else if (left.min == right.min) {
            if (left.sec > right.sec) return true;
            else return false;
        }
        else return false;
    }
    else return false;
}

bool operator<(const Time& left, const Time& right) {
    if (left.hour < right.hour) return true;
    else if (left.hour == right.hour) {
        if (left.min < right.min) return true;
        else if (left.min == right.min) {
            if (left.sec < right.sec) return true;
            else return false;
        }
        else return false;
    }
    else return false;
}

bool operator>=(const Time& left, const Time& right) {
    return left == right || left > right;
}

bool operator<=(const Time& left, const Time& right) {
    return left == right || left < right;
}

Time operator-(const Time& left, const Time& right) {
    return Time(left.toSeconds() - right.toSeconds());
}

Time operator+(const Time& left, const Time& right) {
    return Time(left.toSeconds() + right.toSeconds());
}

Time operator++(Time& t, int) {
    Time oldValue(t.toSeconds());
    t = Time(t.toSeconds() + 1);
    return oldValue;
}

Time operator--(Time& t, int) {
    Time oldValue(t.toSeconds());
    t = Time(t.toSeconds() - 1);
    return oldValue;
}

struct Mealtime {
    Time time;
    std::string title;
};

struct Config {
    Time drinkingPeriod;
    Time breakPeriod;

    std::vector<Mealtime> mealTimes;
    
    Time sleepPeriodStart;
    Time sleepPeriodEnd;
} config;

// Конвертация строки в формате 00:00:00 в Time
Time parseTime(std::string s) {
    Time time;
    int hour = std::stoi(s.substr(0, 2));
    int minute = std::stoi(s.substr(3, 2));
    int second = std::stoi(s.substr(6, 2));

    time.hour = hour;
    time.min = minute;
    time.sec = second;

    return time;
}

std::string beautify(int val) {
    if (val < 10) return "0" + std::to_string(val);
    else return std::to_string(val);
}

std::string timeToString(Time t) {
    std::string s;
    s += beautify(t.hour) + ":";
    s += beautify(t.min) + ":";
    s += beautify(t.sec);

    return s;
}

Config readConfig() {
    std::ifstream file("./Config/config.ini");
    if (!file) {
        std::cout << "Couldn't open config\n";
        exit(1);
    }

    Config cfg;
    std::string line;
    while (std::getline(file, line)) {
        if (line == "drinking:") {
            std::getline(file, line);
            cfg.drinkingPeriod = parseTime(line);
            //std::cout << line << '\n';
        }
        else if (line == "food:") {
            std::getline(file, line);
            while((std::getline(file, line), line) != "]") {
                Mealtime mt;
                // Убираем двоеточие в конце
                line = line.substr(0, line.length() - 1);
                mt.title = line;
                std::getline(file, line);
                mt.time = parseTime(line);
                cfg.mealTimes.push_back(mt);
                //std::cout << line << '\n';
            }
        }
        else if (line == "break:") {
            std::getline(file, line);
            cfg.breakPeriod = parseTime(line);
        }
        else if (line == "sleep:") {
            std::getline(file, line);
            cfg.sleepPeriodStart = parseTime(line);
            //std::cout << line << '\n';
            std::getline(file, line);
            cfg.sleepPeriodEnd = parseTime(line);
            //std::cout << line << '\n';
        }
    }

    return cfg;
}

sf::Color bestStatus(20, 179, 20);
sf::Color worstStatus(200, 20, 20);
sf::Color getStatusColor(int status) {
    int red = map(status, 0, 100, worstStatus.r, bestStatus.r);
    int green = map(status, 0, 100, worstStatus.g, bestStatus.g);
    int blue = map(status, 0, 100, worstStatus.b, bestStatus.b);

    return sf::Color(red, green, blue);
}

const int statusTextY = 55;
void setStringCenter(sf::Text& text, std::string str) {
    text.setString(str);
    sf::FloatRect textBounds = text.getLocalBounds();
    text.setOrigin(textBounds.width / 2, textBounds.height / 2);
    text.setPosition(pokemon_width / 2, statusTextY);
}

int pokemonStatus;
sf::Color backgroundColor;
sf::Text statusText;
bool isCloudActive = false;
void changePokemonStatus(int newStatus) {
    if (newStatus <= 0) newStatus = 0;
    else if (newStatus >= 100) newStatus = 100; 
    pokemonStatus = newStatus;
    backgroundColor = getStatusColor(pokemonStatus);
    if (!isCloudActive) setStringCenter(statusText, "Status: " + std::to_string(newStatus));
}

int rewardForClick = -1;
void setReminder(std::string str, int reward) {
    isCloudActive = true;
    statusText.setFillColor(sf::Color::Black);
    statusText.setOutlineColor(sf::Color::White);
    setStringCenter(statusText, str);

    rewardForClick = reward;
}

//Time noSleepPenaltyPeriod(0, 15, 0);
Time noSleepPenaltyPeriod(0, 0, 30);
void handleTimeEvents(sf::Sound* notification) {
    tm currentTm; 
    Time timeSinceDrinking;
    Time timeSinceBreak;
    Time noSleepTimer;
    bool isSleepTime = false;

    Time lastCycleTime;
    while (!programIsQuitting) {
        currentTm = getCurrentTime();
        Time currentTime = Time(currentTm);

        // Если прошла секунда
        if (currentTime - lastCycleTime > Time(0)) {
            // Если пришло время напоминать пить
            if (timeSinceDrinking == config.drinkingPeriod) {
                std::cout << "Drink some water!\n";
                notification->play();
                // Сбрасываем время
                timeSinceDrinking = Time(0);
                changePokemonStatus(pokemonStatus - 5);
                setReminder("Water!", 5);
            }

            // Время перерыва
            if (timeSinceBreak == config.breakPeriod) {
                std::cout << "Do a break!\n";
                notification->play();
                // Сбрасываем время
                timeSinceBreak = Time(0);
                changePokemonStatus(pokemonStatus - 5);
                setReminder("Break!", 5);
            }

            // Если время совпало с одной из напоминалок о еде
            for (int i = 0; i < config.mealTimes.size(); i++)
                if (currentTime == config.mealTimes[i].time) {
                    std::cout << config.mealTimes[i].title << " time! (" << timeToString(config.mealTimes[i].time) << ")\n";
                    notification->play();
                    changePokemonStatus(pokemonStatus - 10);
                    setReminder(config.mealTimes[i].title, 10);
                    break;
                }

            // Если время спать, каждые noSleepPenaltyPeriod штрафовать очками на день
            if (currentTime >= config.sleepPeriodStart || currentTime <= config.sleepPeriodEnd) {
                // Если мы только зашли на период
                if (!isSleepTime) {
                    std::cout << "Friendly reminder to go to sleep\n";
                    setReminder("Sleep", 0);
                    notification->play();
                    isSleepTime = true;
                }
                if (noSleepTimer == noSleepPenaltyPeriod) {
                    std::cout << "My friend, you need to sleep\n";
                    notification->play();
                    changePokemonStatus(pokemonStatus - 15);
                    setReminder("Sleep!!", 0);
                    // Сбрасываем время
                    noSleepTimer = Time(0);
                }
                else noSleepTimer++;
            }
            else {
                isSleepTime = false;
                timeSinceBreak++;
            }

            timeSinceDrinking++;
            // std::cout << "Second elapsed\n";
        }
        lastCycleTime = currentTime;
        std::this_thread::sleep_for(0.3s);
    }
}

#define ip "localhost"
#define port 8080

std::string login, password;

void writeSession(std::string login, std::string password) {
    std::ofstream fout("./Account/session.txt");
    fout << login << '\n' << password << '\n';
    fout.close();
}

bool sendCommand(sf::TcpSocket& socket, sf::Packet& packet_out, sf::Packet& packet_in, bool isSilent = false) {
    socket.send(packet_out);
    socket.receive(packet_in);
    std::string response;
    packet_in >> response;
    if (response == "success") {
        return true;
    }
    else {
        if (!isSilent) std::cout << "Failed to login, please restart the app.\n";
        return false;
    }
}

void getConfig(sf::TcpSocket& socket, std::string login, std::string password) {
    sf::Packet packet_out, packet_in;
    packet_out << "get_config" << login << password;
    if (sendCommand(socket, packet_out, packet_in)) {
        std::string c;
        packet_in >> c;
        std::ofstream fout("./Config/config.ini");
        fout << c;
        fout.close();
        config = readConfig();
        std::cout << "Downloaded config\n";
    }
}

void sendConfig(sf::TcpSocket& socket, std::string login, std::string password) {
    sf::Packet packet_out, packet_in;
    packet_out << "send_config" << login << password;

    std::ifstream fin("./Config/config.ini");
    if (fin) {
        std::stringstream buffer;
        buffer << fin.rdbuf();
        std::string c = buffer.str();

        packet_out << c;
        if (sendCommand(socket, packet_out, packet_in)) {
            std::cout << "Uploaded config\n";
        }
    }
    else {
        std::cout << "Can't open config... Does it exist/Is it in use by another program?\n";
    }
}

bool testLogin(sf::TcpSocket& socket, std::string login, std::string password) {
    sf::Packet packet_out, packet_in;
    packet_out << "test_login" << login << password;
    return sendCommand(socket, packet_out, packet_in, true);
}

void loginOrRegister(sf::TcpSocket& socket);
void firstLogin(sf::TcpSocket& socket, std::string login, std::string password) {
    if (testLogin(socket, login, password)) {
        std::cout << "Logged in successfuly\n";
    }
    else {
        loginOrRegister(socket);
    }
}

bool Register(sf::TcpSocket& socket, std::string login, std::string password) {
    sf::Packet packet_out, packet_in;
    packet_out << "create_user" << login << password;
    return sendCommand(socket, packet_out, packet_in, true);
}

void loginOrRegister(sf::TcpSocket& socket) {
    char choice;
    std::cout << "You wish to:\n1) Login\n2) Register\n\nChoice: ";
    bool isChoiceOk = true;
    do {
        isChoiceOk = true;
        std::cin >> choice;
        switch (choice) {
        case '1': {
            bool loginSuccessful = false;
            do {
                std::cout << "Enter your login: " << '\n';
                std::cin >> login;
                std::cout << "Enter your password: " << '\n';
                std::cin >> password;
                loginSuccessful = testLogin(socket, login, password);
                if (!loginSuccessful) std::cout << "Login/Password incorrect\n";
            } while (!loginSuccessful);
            writeSession(login, password);
            break;
        }
        case '2': {
            bool registerSuccessful = false;
            do {
                std::cout << "Enter your login: " << '\n';
                std::cin >> login;
                std::cout << "Enter your password: " << '\n';
                std::cin >> password;
                registerSuccessful = Register(socket, login, password);
                if (!registerSuccessful) std::cout << "User exists, please try again\n";
            } while (!registerSuccessful);
            writeSession(login, password);
            getConfig(socket, login, password);
            break;
        }
        default:
            std::cout << "Incorrect choice, try again;\nChoice: ";
            isChoiceOk = false;
            break;
        }
    } while (!isChoiceOk);
}

int main() {
    showConsole();
    // Перед созданием окна нужно подключиться к серверу
    sf::TcpSocket socket;
    bool connectionDone = false, offlineMode = false;
    while (!connectionDone) {
        if (socket.connect(ip, port) != sf::Socket::Done) {
            char choice;
            std::cout << "Can't connect to server\n1) Try again;\n2) Offline mode\n\nChoice: ";
            bool isChoiceOk = true;
            do {
                isChoiceOk = true;
                std::cin >> choice;
                switch (choice) {
                case '1':
                    break;
                case '2':
                    offlineMode = true;
                    connectionDone = true;
                    break;
                default:
                    std::cout << "Incorrect choice, try again;\nChoice: ";
                    isChoiceOk = false;
                    break;
                }
            } while (!isChoiceOk);
        }
        else connectionDone = true;
    }

    if (!offlineMode) {
        std::ifstream fin("./Account/session.txt");
        if (!fin) {
            // Сессии не существует, значит пользователь не зарегистрирован/залогинен
            loginOrRegister(socket);
        }
        else {
            // Пробуем использовать сессию для авторизации
            fin >> login >> password;
            fin.close();
            
            firstLogin(socket, login, password);
        }
    }

    #ifndef DEBUG
        hideConsole();
    #endif

    // Настройка окна и основного спрайта
    sf::RenderWindow window;

    sf::Texture herotexture; 
    herotexture.loadFromFile("./Eevee/go.png");
    sf::Sprite herosprite;
    herosprite.setTexture(herotexture);
    herosprite.setScale(2, 2);
    sf::FloatRect spriteRect = herosprite.getGlobalBounds();
    pokemon_width = spriteRect.width;
    pokemon_height = spriteRect.height;
    window.create(sf::VideoMode(pokemon_width, pokemon_height), "Tamagotchi", sf::Style::None);
    // std::cout << spriteRect.width << " " << spriteRect.height << '\n';

    window.setVerticalSyncEnabled(true);
    makeWindowTransparent(window);
    makeWindowOnTop(window);
    setWindowAlpha(window, 255);

    // Облако
    sf::Texture cloudtexture;
    cloudtexture.loadFromFile("./Texture/cloud.png");
    sf::Sprite cloud;
    cloud.setTexture(cloudtexture);
    cloud.setScale(0.47, 0.35);
    sf::FloatRect cloudBounds = cloud.getLocalBounds();
    cloud.setOrigin(cloudBounds.width / 2, cloudBounds.height / 2);
    cloud.setPosition(pokemon_width / 2, 65);

    // Настройка курсоров
    sf::Cursor arrowCursor;
    arrowCursor.loadFromSystem(sf::Cursor::Arrow);
    sf::Cursor handCursor;
    handCursor.loadFromSystem(sf::Cursor::Hand);

    // Кнопка
    // bool isButtonActive = true;
    const int btnWidth = 35;
    const int btnHeight = btnWidth;
    sf::RectangleShape btn(sf::Vector2f(btnWidth, btnHeight));
    btn.setOutlineThickness(1);
    btn.setOutlineColor(sf::Color::Black);
    btn.setFillColor(sf::Color::White);
    sf::Texture btntexture;
    btntexture.loadFromFile("./Texture/tick.png");
    btn.setTexture(&btntexture);
    const int btnX = 20;
    const int btnY = pokemon_height / 2 + 10;
    btn.setPosition(btnX, btnY);

    // Перемещение окна
    sf::Vector2i grabbedOffset;
    bool isWindowGrabbed = false;

    // Шрифты
    sf::Font regular;
    if (!regular.loadFromFile("./fonts/progresspixel-bold.ttf")) {
        std::cout << "Can't open progresspixel-bold.ttf\n";
        system("PAUSE");
        return 1;
    }

    // Звуки уведомлений
    sf::SoundBuffer buffer;
    if (!buffer.loadFromFile("./Notifications/5.wav")) {
        std::cout << "Can't load notification sound\n";
        system("PAUSE");
        return 1;
    }
    sf::Sound notification;
    notification.setBuffer(buffer);
    notification.setVolume(70.f);
    // Состояние покемона (0 - 100)
    changePokemonStatus(100);
    statusText.setFont(regular);
    statusText.setCharacterSize(30);
    statusText.setFillColor(sf::Color::White);
    statusText.setOutlineColor(sf::Color::Black);
    statusText.setOutlineThickness(1);
    sf::FloatRect textBounds = statusText.getLocalBounds();
    statusText.setOrigin(textBounds.width / 2, textBounds.height / 2);
    statusText.setPosition(pokemon_width / 2, statusTextY);

    // Чтение конфига
    config = readConfig();
    std::cout << timeToString(config.drinkingPeriod) << '\n';
    for (int i = 0; i < config.mealTimes.size(); i++) 
        std::cout << config.mealTimes[i].title << " " << timeToString(config.mealTimes[i].time) << '\n';
    std::cout << timeToString(config.sleepPeriodStart) << '\n';
    std::cout << timeToString(config.sleepPeriodEnd) << '\n';

    // Запуск обработки временных событий
    std::thread timeThread(handleTimeEvents, &notification);

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            switch (event.type) {
            case sf::Event::Closed:
                closeWindow(window);
                break;
            case sf::Event::MouseButtonPressed:
                if (event.mouseButton.button == sf::Mouse::Left) {
                    grabbedOffset = window.getPosition() - sf::Mouse::getPosition();
                    isWindowGrabbed = true;
                }
                break;
            case sf::Event::MouseButtonReleased:
                if (event.mouseButton.button == sf::Mouse::Left)
                    isWindowGrabbed = false;
                break;
            case sf::Event::MouseMoved:
                mouseX = event.mouseMove.x;
                mouseY = event.mouseMove.y;

                if (isWindowGrabbed) {
                    window.setPosition(sf::Mouse::getPosition() + grabbedOffset);
                }
                break;
            case sf::Event::MouseEntered:
                setWindowAlpha(window, 255); 
                break;
            case sf::Event::MouseLeft:
                setWindowAlpha(window, 50);
                break;

            case sf::Event::KeyPressed:
                // Ctrl + Q
                if (event.key.control && event.key.code == sf::Keyboard::Q) {
                    closeWindow(window);
                }
                // ~
                else if (event.key.code == sf::Keyboard::Tilde) {
                    showConsole();

                    system("cls");
                    std::cout << "What do you wish to do?\n1) Upload config file to the server;\n";
                    std::cout << "2) Download config file from the server;\n3) Log out from account;\n";
                    std::cout << "4) Close the program;\n5) Exit this menu.\nChoice: ";

                    char choice;
                    bool isChoiceOk = true;
                    do {
                        isChoiceOk = true;
                        std::cin >> choice;
                        switch (choice) {
                        case '1':
                            std::cout << "This will override the config on the server. Are you sure?\n(y/n): ";
                            std::cin >> choice;
                            if (choice == 'y' || choice == 'Y') {
                                std::cout << "Sending the config...";
                                sendConfig(socket, login, password);
                            }
                            else {
                                std::cout << "Choice: ";
                                isChoiceOk = false;
                            }
                            break;
                        case '2':
                            std::cout << "This will override the config on your PC. Are you sure?\n(y/n): ";
                            std::cin >> choice;
                            if (choice == 'y' || choice == 'Y') {
                                std::cout << "Getting the config...";
                                getConfig(socket, login, password);
                            }
                            else {
                                std::cout << "Choice: ";
                                isChoiceOk = false;
                            }
                            break;
                        case '3':
                            system("del Account\\session.txt");
                            loginOrRegister(socket);
                            break;
                        case '4':
                            window.close();
                            exit(0);
                            break;
                        case '5':
                            break;

                        default:
                            isChoiceOk = false;
                            std::cout << "Incorrect choice\nChoice: ";
                        }
                    } while (!isChoiceOk);

                    #ifndef DEBUG
                    hideConsole();
                    #endif
                }
                break;
            }

        }
        // clear the window with black color
        window.clear(backgroundColor);
        window.draw(herosprite);
        if (isCloudActive) {
            window.draw(cloud);
            window.draw(btn);
            if (mouseX >= btnX && mouseX <= btnX + btnWidth &&
                mouseY >= btnY && mouseY <= btnY + btnHeight)
            {
                window.setMouseCursor(handCursor);
                if (sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
                    std::cout << "Ok, nice\n";
                    isCloudActive = false;
                    changePokemonStatus(pokemonStatus + rewardForClick);
                    statusText.setFillColor(sf::Color::White);
                    statusText.setOutlineColor(sf::Color::Black);
                }
            }
            else window.setMouseCursor(arrowCursor);
        }
        else window.setMouseCursor(arrowCursor);

        window.draw(statusText);


        // end the current frame 
        window.display();
    }
  
    // Ожидаем, пока завершится поток с временными событиями
    timeThread.join();

    return 0;
}