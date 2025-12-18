#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <fstream>
#include <random>
#include <thread>

using std::string;
using std::vector;
using std::shared_ptr;

// Константы для карты
const int MAP_WIDTH = 100;
const int MAP_HEIGHT = 100;
const int GAME_DURATION_SECONDS = 30;

// Глобальный мьютекс для cout
extern std::mutex cout_mutex;

//================ Observer ================
class Observer
{
public:
    virtual ~Observer() = default;
    virtual void on_kill(const string& killer, const string& victim) = 0;
};

class ConsoleObserver : public Observer
{
public:
    void on_kill(const string& killer, const string& victim) override;
};

class FileObserver : public Observer
{
public:
    explicit FileObserver(const string& filename);
    ~FileObserver();
    void on_kill(const string& killer, const string& victim) override;

private:
    std::ofstream* file;
    std::mutex file_mutex;
};

//================ Visitor ================
class Orc;
class Bear;
class Squirrel;

class Visitor
{
public:
    virtual ~Visitor() = default;
    virtual void visit(Orc&) = 0;
    virtual void visit(Bear&) = 0;
    virtual void visit(Squirrel&) = 0;
};

//================ NPC ====================
class NPC
{
protected:
    string name;
    int x;
    int y;
    bool alive;
    mutable std::shared_mutex mtx;

public:
    NPC(const string& name, int x, int y);
    virtual ~NPC() = default;

    virtual string type() const = 0;
    virtual void accept(Visitor& v) = 0;
    virtual int get_move_distance() const = 0;
    virtual int get_kill_distance() const = 0;
    
    // Потокобезопасные методы
    double distance_to(int other_x, int other_y) const;
    double distance_to(const NPC& other) const;
    bool is_alive() const;
    void kill();
    
    // Методы для перемещения
    void move(int dx, int dy);
    void move_random(std::mt19937& gen);
    
    // Геттеры с блокировкой
    string get_name() const;
    std::pair<int, int> get_position() const;
    int get_x() const;
    int get_y() const;
    
    // Для отрисовки карты
    char get_symbol() const;
};

class Orc : public NPC
{
public:
    Orc(const string& name, int x, int y);
    string type() const override;
    void accept(Visitor& v) override;
    int get_move_distance() const override;
    int get_kill_distance() const override;
};

class Bear : public NPC
{
public:
    Bear(const string& name, int x, int y);
    string type() const override;
    void accept(Visitor& v) override;
    int get_move_distance() const override;
    int get_kill_distance() const override;
};

class Squirrel : public NPC
{
public:
    Squirrel(const string& name, int x, int y);
    string type() const override;
    void accept(Visitor& v) override;
    int get_move_distance() const override;
    int get_kill_distance() const override;
};

//================ Factory =================
class NPCFactory
{
public:
    static shared_ptr<NPC> create(const string& type,
                                  const string& name,
                                  int x,
                                  int y);
    static shared_ptr<NPC> create_random(const string& type_prefix, std::mt19937& gen);
};

//================ Battle ==================
class BattleVisitor : public Visitor
{
public:
    BattleVisitor(NPC& attacker,
                  vector<shared_ptr<Observer>>& observers,
                  std::mt19937& gen);

    void visit(Orc&) override;
    void visit(Bear&) override;
    void visit(Squirrel&) override;

private:
    NPC& attacker;
    vector<shared_ptr<Observer>>& observers;
    std::mt19937& gen;
    std::uniform_int_distribution<int> dice{1, 6};
    
    void notify(const string& victim);
    bool roll_dice_battle();
};

//================ Game Manager ============
class GameManager
{
private:
    vector<shared_ptr<NPC>> npcs;
    vector<shared_ptr<Observer>> observers;
    std::atomic<bool> game_running{false};
    mutable std::mutex npcs_mutex; // Добавлено mutable
    
    // Потоки
    std::thread movement_thread;
    std::thread battle_thread;
    std::thread display_thread;
    
public:
    GameManager();
    ~GameManager();
    
    void initialize_game();
    void start_game();
    void stop_game();
    void run_game();
    
    void movement_worker();
    void battle_worker();
    void display_worker();
    
    void print_survivors() const;
    void print_map() const;
    
    // Для тестирования
    void add_npc(shared_ptr<NPC> npc);
    void add_observer(shared_ptr<Observer> observer);
};

//================ File ops ================
void save_to_file(const vector<shared_ptr<NPC>>& npcs);
void load_from_file(vector<shared_ptr<NPC>>& npcs);

#endif // FUNCTIONS_H