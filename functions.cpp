#include "functions.h"
#include <cmath>
#include <fstream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <random>
#include <iomanip>
#include <cctype>

using std::cout;
using std::endl;
using std::make_shared;

// Глобальный мьютекс для cout
std::mutex cout_mutex;

//================ Observer =================
void ConsoleObserver::on_kill(const string& killer, const string& victim)
{
    std::lock_guard<std::mutex> lock(cout_mutex);
    cout << "[BATTLE] " << killer << " killed " << victim << endl;
}

FileObserver::FileObserver(const string& filename)
{
    file = new std::ofstream(filename, std::ios::app);
}

FileObserver::~FileObserver()
{
    if (file)
    {
        file->close();
        delete file;
    }
}

void FileObserver::on_kill(const string& killer, const string& victim)
{
    std::lock_guard<std::mutex> lock(file_mutex);
    if (file && *file)
    {
        (*file) << killer << " killed " << victim << endl;
    }
}

//================ NPC ======================
NPC::NPC(const string& n, int px, int py)
    : name(n), x(px), y(py), alive(true) {}

double NPC::distance_to(int other_x, int other_y) const
{
    std::shared_lock<std::shared_mutex> lock(mtx);
    return std::sqrt((x - other_x)*(x - other_x) + (y - other_y)*(y - other_y));
}

double NPC::distance_to(const NPC& o) const
{
    auto [other_x, other_y] = o.get_position();
    return distance_to(other_x, other_y);
}

bool NPC::is_alive() const 
{ 
    std::shared_lock<std::shared_mutex> lock(mtx);
    return alive; 
}

void NPC::kill() 
{ 
    std::unique_lock<std::shared_mutex> lock(mtx);
    alive = false; 
}

void NPC::move(int dx, int dy)
{
    std::unique_lock<std::shared_mutex> lock(mtx);
    if (!alive) return;
    
    int new_x = x + dx;
    int new_y = y + dy;
    
    // Проверка границ карты
    if (new_x >= 0 && new_x < MAP_WIDTH && new_y >= 0 && new_y < MAP_HEIGHT)
    {
        x = new_x;
        y = new_y;
    }
}

void NPC::move_random(std::mt19937& gen)
{
    if (!is_alive()) return;
    
    std::uniform_int_distribution<int> dir_dist(-1, 1);
    int dx = dir_dist(gen);
    int dy = dir_dist(gen);
    
    move(dx, dy);
}

string NPC::get_name() const 
{ 
    std::shared_lock<std::shared_mutex> lock(mtx);
    return name; 
}

std::pair<int, int> NPC::get_position() const
{
    std::shared_lock<std::shared_mutex> lock(mtx);
    return {x, y};
}

int NPC::get_x() const 
{ 
    std::shared_lock<std::shared_mutex> lock(mtx);
    return x; 
}

int NPC::get_y() const 
{ 
    std::shared_lock<std::shared_mutex> lock(mtx);
    return y; 
}

char NPC::get_symbol() const
{
    std::shared_lock<std::shared_mutex> lock(mtx);
    if (!alive) return ' ';
    
    char type_char = type()[0];
    return std::toupper(static_cast<unsigned char>(type_char));
}

//================ Orc ======================
Orc::Orc(const string& n, int x, int y) : NPC(n, x, y) {}
string Orc::type() const { return "Orc"; }
int Orc::get_move_distance() const { return 20; }
int Orc::get_kill_distance() const { return 10; }
void Orc::accept(Visitor& v) { v.visit(*this); }

//================ Bear =====================
Bear::Bear(const string& n, int x, int y) : NPC(n, x, y) {}
string Bear::type() const { return "Bear"; }
int Bear::get_move_distance() const { return 5; }
int Bear::get_kill_distance() const { return 10; }
void Bear::accept(Visitor& v) { v.visit(*this); }

//================ Squirrel =================
Squirrel::Squirrel(const string& n, int x, int y) : NPC(n, x, y) {}
string Squirrel::type() const { return "Squirrel"; }
int Squirrel::get_move_distance() const { return 5; }
int Squirrel::get_kill_distance() const { return 5; }
void Squirrel::accept(Visitor& v) { v.visit(*this); }

//================ Factory ==================
shared_ptr<NPC> NPCFactory::create(const string& type,
                                   const string& name,
                                   int x,
                                   int y)
{
    if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT)
        throw std::runtime_error("Координаты вне диапазона карты");

    if (type == "Orc") return make_shared<Orc>(name, x, y);
    if (type == "Bear") return make_shared<Bear>(name, x, y);
    if (type == "Squirrel") return make_shared<Squirrel>(name, x, y);

    throw std::runtime_error("Неизвестный тип NPC");
}

shared_ptr<NPC> NPCFactory::create_random(const string& type_prefix, std::mt19937& gen)
{
    static int counter = 0;
    std::uniform_int_distribution<int> coord_x(0, MAP_WIDTH - 1);
    std::uniform_int_distribution<int> coord_y(0, MAP_HEIGHT - 1);
    
    vector<string> types = {"Orc", "Bear", "Squirrel"};
    std::uniform_int_distribution<int> type_dist(0, types.size() - 1);
    
    string type = types[type_dist(gen)];
    string name = type_prefix + std::to_string(++counter);
    int x = coord_x(gen);
    int y = coord_y(gen);
    
    return create(type, name, x, y);
}

//================ Battle ===================
BattleVisitor::BattleVisitor(NPC& a,
                             vector<shared_ptr<Observer>>& o,
                             std::mt19937& g)
    : attacker(a), observers(o), gen(g) {}

bool BattleVisitor::roll_dice_battle()
{
    int attack_power = dice(gen);
    int defense_power = dice(gen);
    
    return attack_power > defense_power;
}

void BattleVisitor::notify(const string& victim)
{
    for (auto& obs : observers)
        obs->on_kill(attacker.get_name(), victim);
}

void BattleVisitor::visit(Orc& npc)
{
    if (!npc.is_alive()) return;
    
    if (attacker.type() == "Orc" || attacker.type() == "Bear")
    {
        if (roll_dice_battle())
        {
            npc.kill(); 
            notify(npc.get_name());
        }
    }
}

void BattleVisitor::visit(Bear& npc)
{
    if (!npc.is_alive()) return;
    
    if (attacker.type() == "Orc")
    {
        if (roll_dice_battle())
        {
            npc.kill(); 
            notify(npc.get_name());
        }
    }
}

void BattleVisitor::visit(Squirrel& npc)
{
    // Белки не атакуют и не могут быть атакованы по текущей логике
    (void)npc; // Используем параметр, чтобы избежать предупреждения
}

//================ Game Manager =============
GameManager::GameManager()
{
    observers.push_back(make_shared<ConsoleObserver>());
    observers.push_back(make_shared<FileObserver>("battle_log.txt"));
}

GameManager::~GameManager()
{
    stop_game();
}

void GameManager::initialize_game()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    
    {
        std::lock_guard<std::mutex> lock(npcs_mutex);
        npcs.clear();
        
        // Создаем 50 случайных NPC
        for (int i = 0; i < 50; ++i)
        {
            npcs.push_back(NPCFactory::create_random("NPC_", gen));
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        cout << "Игра инициализирована. Создано 50 NPC." << endl;
    }
}

void GameManager::start_game()
{
    game_running = true;
    
    // Запускаем потоки
    movement_thread = std::thread(&GameManager::movement_worker, this);
    battle_thread = std::thread(&GameManager::battle_worker, this);
    display_thread = std::thread(&GameManager::display_worker, this);
    
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        cout << "Игра началась! Длительность: " << GAME_DURATION_SECONDS << " секунд." << endl;
    }
}

void GameManager::stop_game()
{
    game_running = false;
    
    if (movement_thread.joinable()) movement_thread.join();
    if (battle_thread.joinable()) battle_thread.join();
    if (display_thread.joinable()) display_thread.join();
    
    print_survivors();
}

void GameManager::run_game()
{
    initialize_game();
    start_game();
    
    // Ждем указанное время
    std::this_thread::sleep_for(std::chrono::seconds(GAME_DURATION_SECONDS));
    
    stop_game();
}

void GameManager::movement_worker()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    
    while (game_running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 10 раз в секунду
        
        std::lock_guard<std::mutex> lock(npcs_mutex);
        
        for (auto& npc : npcs)
        {
            if (npc->is_alive())
            {
                // Перемещаем NPC
                npc->move_random(gen);
            }
        }
    }
}

void GameManager::battle_worker()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    
    while (game_running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 5 раз в секунду
        
        std::lock_guard<std::mutex> lock(npcs_mutex);
        
        // Проверяем все пары NPC на возможность боя
        for (size_t i = 0; i < npcs.size(); ++i)
        {
            if (!npcs[i]->is_alive()) continue;
            
            for (size_t j = i + 1; j < npcs.size(); ++j)
            {
                if (!npcs[j]->is_alive()) continue;
                
                double distance = npcs[i]->distance_to(*npcs[j]);
                
                // Проверяем, могут ли NPC атаковать друг друга
                if (distance <= npcs[i]->get_kill_distance() && 
                    distance <= npcs[j]->get_kill_distance())
                {
                    // NPC i атакует NPC j
                    BattleVisitor visitor_i(*npcs[i], observers, gen);
                    npcs[j]->accept(visitor_i);
                    
                    // NPC j атакует NPC i (если выжил)
                    if (npcs[i]->is_alive() && npcs[j]->is_alive())
                    {
                        BattleVisitor visitor_j(*npcs[j], observers, gen);
                        npcs[i]->accept(visitor_j);
                    }
                }
            }
        }
        
        // Удаляем мертвых NPC
        npcs.erase(std::remove_if(npcs.begin(), npcs.end(),
            [](auto& n){ return !n->is_alive(); }), npcs.end());
    }
}

void GameManager::display_worker()
{
    while (game_running)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Раз в секунду
        
        print_map();
        
        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            cout << "Живых NPC: ";
            
            std::lock_guard<std::mutex> npc_lock(npcs_mutex);
            int alive_count = 0;
            for (auto& npc : npcs)
            {
                if (npc->is_alive()) ++alive_count;
            }
            cout << alive_count << endl;
        }
    }
}

void GameManager::print_survivors() const
{
    std::lock_guard<std::mutex> cout_lock(cout_mutex);
    std::lock_guard<std::mutex> npc_lock(npcs_mutex);
    
    cout << "\n=== ВЫЖИВШИЕ NPC ===" << endl;
    cout << "Всего выжило: " << npcs.size() << endl;
    
    for (auto& npc : npcs)
    {
        if (npc->is_alive())
        {
            auto [x, y] = npc->get_position();
            cout << npc->type() << " " << npc->get_name() 
                 << " (" << x << "," << y << ")" << endl;
        }
    }
    cout << "===================\n" << endl;
}

void GameManager::print_map() const
{
    std::lock_guard<std::mutex> cout_lock(cout_mutex);
    std::lock_guard<std::mutex> npc_lock(npcs_mutex);
    
    // Создаем карту
    vector<vector<char>> map(MAP_HEIGHT, vector<char>(MAP_WIDTH, '.'));
    
    // Размещаем NPC на карте
    for (auto& npc : npcs)
    {
        if (npc->is_alive())
        {
            auto [x, y] = npc->get_position();
            if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT)
            {
                map[y][x] = npc->get_symbol();
            }
        }
    }
    
    // Выводим карту
    cout << "\n=== КАРТА ===" << endl;
    for (int y = 0; y < MAP_HEIGHT; ++y)
    {
        for (int x = 0; x < MAP_WIDTH; ++x)
        {
            cout << map[y][x];
        }
        cout << endl;
    }
    cout << "============\n" << endl;
}

void GameManager::add_npc(shared_ptr<NPC> npc)
{
    std::lock_guard<std::mutex> lock(npcs_mutex);
    npcs.push_back(npc);
}

void GameManager::add_observer(shared_ptr<Observer> observer)
{
    observers.push_back(observer);
}

//================ File ops =================
void save_to_file(const vector<shared_ptr<NPC>>& npcs)
{
    std::ofstream out("npcs.txt");
    for (auto& n : npcs)
        if (n->is_alive())
            out << n->type() << " " << n->get_name() << " "
                << n->get_x() << " " << n->get_y() << endl;
}

void load_from_file(vector<shared_ptr<NPC>>& npcs)
{
    std::ifstream in("npcs.txt");
    npcs.clear();
    string type, name;
    int x, y;
    while (in >> type >> name >> x >> y)
        npcs.push_back(NPCFactory::create(type, name, x, y));
}