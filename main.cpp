#include "functions.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <thread>

using std::cin;
using std::cout;
using std::endl;
using std::make_shared;

void run_simulation()
{
    GameManager game;
    
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        cout << "Запуск симуляции..." << endl;
        cout << "Длительность: " << GAME_DURATION_SECONDS << " секунд" << endl;
        cout << "Размер карты: " << MAP_WIDTH << "x" << MAP_HEIGHT << endl;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    game.run_game();
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        cout << "Симуляция завершена за " << duration.count() << " секунд." << endl;
    }
}

int main()
{
    vector<shared_ptr<NPC>> npcs;
    vector<shared_ptr<Observer>> observers{
        make_shared<ConsoleObserver>(),
        make_shared<FileObserver>("log.txt")
    };

    int choice;
    do
    {
        cout << "\n=== ГЛАВНОЕ МЕНЮ ===" << endl;
        cout << "1 - Добавить NPC" << endl;
        cout << "2 - Показать NPC" << endl;
        cout << "3 - Сохранить" << endl;
        cout << "4 - Загрузить" << endl;
        cout << "5 - Запуск боя (одиночный раунд)" << endl;
        cout << "6 - Запуск полной симуляции (30 секунд)" << endl;
        cout << "0 - Выход" << endl;
        cout << "Выбор: ";
        cin >> choice;

        if (choice == 1)
        {
            string type, name;
            int x, y;
            cout << "Тип (Orc/Bear/Squirrel): "; cin >> type;
            cout << "Имя: "; cin >> name;
            cout << "x y (0-" << MAP_WIDTH-1 << " 0-" << MAP_HEIGHT-1 << "): "; 
            cin >> x >> y;
            try
            {
                npcs.push_back(NPCFactory::create(type, name, x, y));
                cout << "NPC создан!" << endl;
            }
            catch (const std::exception& e)
            {
                cout << "Ошибка: " << e.what() << endl;
            }
        }
        else if (choice == 2)
        {
            cout << "\n=== СПИСОК NPC ===" << endl;
            for (auto& n : npcs)
            {
                auto [x, y] = n->get_position();
                cout << n->type() << " " << n->get_name()
                     << " (" << x << "," << y << ") "
                     << (n->is_alive() ? "жив" : "мертв") << endl;
            }
        }
        else if (choice == 3)
        {
            save_to_file(npcs);
            cout << "Сохранено в npcs.txt" << endl;
        }
        else if (choice == 4)
        {
            try
            {
                load_from_file(npcs);
                cout << "Загружено из npcs.txt" << endl;
            }
            catch (const std::exception& e)
            {
                cout << "Ошибка загрузки: " << e.what() << endl;
            }
        }
        else if (choice == 5)
        {
            double range;
            cout << "Дальность боя: "; cin >> range;
            
            std::random_device rd;
            std::mt19937 gen(rd());
            
            for (auto& a : npcs)
                for (auto& b : npcs)
                    if (a != b && a->is_alive() && b->is_alive() &&
                        a->distance_to(*b) <= range)
                    {
                        BattleVisitor v(*a, observers, gen);
                        b->accept(v);
                    }

            npcs.erase(std::remove_if(npcs.begin(), npcs.end(),
                [](auto& n){ return !n->is_alive(); }), npcs.end());
            
            cout << "Бой завершен!" << endl;
        }
        else if (choice == 6)
        {
            run_simulation();
        }

    } while (choice != 0);

    cout << "Выход из программы..." << endl;
    return 0;
}