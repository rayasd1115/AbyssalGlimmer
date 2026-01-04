#include "rlutil.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <vector>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <string>
#include <algorithm>
#include <cmath>
#include <locale>
#include <sstream>

// Map size
const int WIDTH = 40;
const int HEIGHT = 20;

enum Tile { FLOOR = 0, WALL = 1 };

enum ItemType { IT_NONE = 0, IT_POTION = 1, IT_COIN = 2 };

struct Item {
    int x, y;
    ItemType type;
    int value;
};

struct Enemy {
    int x, y;
    int hp;
    int dmg;
    int cooldown; // ticks until next action
};

struct Player {
    int x, y;
    int hp;
    int gold;
    int meleeAtk; // melee attack damage
    int rangedAtk; // ranged attack damage
    int rangedAtkRange; // ranged attack range
    int dirX;   // facing direction
    int dirY;
    int shield; // current shield points
    int maxShield; // maximum shield capacity
};

struct Projectile {
    int x, y;
    int dx, dy;
    int range;
    int dmg;
};

struct Effect {
    int x, y;
    char glyph;
    int color;
    int lifetime; // in frames/ticks
};

struct Merchant {

    int x, y;

};



struct Boss {



    int hp;



    int phase;



    std::vector<std::pair<int, int>> bodyParts;



};







struct BossProjectile {



    int x, y;



    int dx, dy;



    int dmg;



    int range;



};







class Game {



private:



    int map[WIDTH][HEIGHT];



    bool visited[WIDTH][HEIGHT];



    Player player;



    std::vector<Item> items;



    std::vector<Enemy> enemies;



    std::vector<Projectile> projectiles;



    std::vector<Effect> effects;



    std::vector<BossProjectile> bossProjectiles;



    Merchant merchant;



    Boss boss;



    int exitX, exitY;



    int level;



    bool running;



    std::vector<std::string> logLines;







    // Double-buffering helpers to avoid full-screen flicker



    char prevGlyph[WIDTH][HEIGHT];



    int prevColor[WIDTH][HEIGHT];



    int prevPlayerHP = -1;



    int prevPlayerGold = -1;



    int prevLevel = -1;



    int prevPlayerMelee = -1;



    int prevPlayerRanged = -1;



    int prevPlayerShield = -1;



    int prevPlayerMaxShield = -1;







    // Shield regen control



    int shieldRegenCounter = 0;



    const int SHIELD_REGEN_INTERVAL = 10; // ticks per regen (3 points every 0.6s = 5/sec)



    const int SHIELD_REGEN_AMOUNT = 3;



    std::vector<std::string> prevLogLines;







    // Enemy sight radius (fixed since Light is removed)



    const int ENEMY_SIGHT_RADIUS = 8;



    const int ENEMY_MOVE_DELAY = 3; // number of ticks enemies wait between actions







    // Gold cap



    const int GOLD_MAX = 1000;







    // Enemy spawn control: don't spawn at start; spawn after player moves a bit



    bool enemiesSpawned = false;



    int movesSinceStart = 0;



    const int ENEMY_SPAWN_THRESHOLD = 3; // spawn after this many moves



    const int DELAYED_ENEMY_COUNT = 3;   // fewer enemies when they first appear







    // Intro screen control



    bool introShown = false;



    // Ranged upgrade cost (increases by 5 after each purchase)



    int upgradeCost = 20;







    // Shop / merchant for level 5



    bool isShop = false;







    // Boss level



    bool isBossLevel = false;



    bool bossDefeated = false;



    int bossAttackCounter = 0;



    int bossAttackDirection = 0; // 0-7 for N, NE, E, SE, S, SW, W, NW



        int transitionCounter = 0;



        int bossMoveCooldown = 0;



        int playerOnBossCooldown = 0;



    



    public:



        Game() {





        std::srand((unsigned)std::time(nullptr));

        level = 1;

        running = true;

        // start with zero gold only when a new game is created

        player.gold = 0;

        // initial attack values set here so upgrades persist across levels

        player.meleeAtk = 8;

        player.rangedAtk = 8;

        player.rangedAtkRange = 6;

        // initial HP set only at new game start so HP persists across levels

        player.hp = 100;

                // shield starts at 0; purchases increase maxShield and current shield

                player.shield = 0;

                player.maxShield = 0;

                init();

            }

        

                        void init() {

        

                            if (level == 6) {

        

                                setupBossLevel();

        

                                return; // Skip rest of normal init

        

                            }

        

                    

        

                            isBossLevel = false;

        

                            isShop = false;

        

                    

        

                            // Initialize player

        

                            player.x = WIDTH / 2;

        

                            player.y = HEIGHT / 2;

        

                            // do NOT reset player.hp here so HP persists across levels

        

                            // do NOT reset player.gold here so gold persists across levels

        

                            // NOTE: melee & ranged attack values are initialized in the constructor so upgrades persist across levels

        

                            // default facing: up

        

                            player.dirX = 0; player.dirY = -1;

        

                    

        

                            // Initialize map with all walls

        

                            for (int i = 0; i < WIDTH; ++i)

        

                                for (int j = 0; j < HEIGHT; ++j) {

        

                                    map[i][j] = WALL;

        

                                    visited[i][j] = false;

        

                                }

        

                // Reset previous-frame buffers so first frame fully redraws

                for (int i = 0; i < WIDTH; ++i)

                    for (int j = 0; j < HEIGHT; ++j) {

                        prevGlyph[i][j] = '\0';

                        prevColor[i][j] = -2;

                    }

                prevLogLines.clear();

                prevPlayerHP = prevPlayerGold = prevLevel = -1;

                prevPlayerMelee = prevPlayerRanged = -1;

        

                // Enemy spawn state

                enemies.clear();

                enemiesSpawned = false;

                movesSinceStart = 0;

        

                generateMap();

                placeItemsAndEnemies();

                // If this is the shop level, configure special shop

                if (level == 5) setupShop();

                message("Welcome to Abyssal Glimmer! Use W/A/S/D to move, 'F' to attack, ESC to quit.");

            }

    void message(const std::string &s) {
        logLines.push_back(s);
        if (logLines.size() > 3) logLines.erase(logLines.begin());
    }

    void generateMap() {
        // use a simple drunkard walk to carve out floor
        int targetFloor = (WIDTH * HEIGHT) / 2; // target floor count
        int px = player.x, py = player.y;
        map[px][py] = FLOOR;
        int count = 1;
        while (count < targetFloor) {
            int dir = std::rand() % 4;
            if (dir == 0 && px + 1 < WIDTH) px++;
            else if (dir == 1 && px - 1 >= 0) px--;
            else if (dir == 2 && py + 1 < HEIGHT) py++;
            else if (dir == 3 && py - 1 >= 0) py--;
            if (map[px][py] == WALL) {
                map[px][py] = FLOOR;
                count++;
            }
        }

        // place the exit on a random floor tile
        while (true) {
            int rx = std::rand() % WIDTH;
            int ry = std::rand() % HEIGHT;
            if (map[rx][ry] == FLOOR && (rx != player.x || ry != player.y)) {
                exitX = rx; exitY = ry; break;
            }
        }
    }

    // Show a more detailed instruction screen and wait for a keypress
    void showInstructions() {
        rlutil::cls();
        rlutil::locate(2, 2);
        rlutil::setColor(rlutil::LIGHTGREEN);
        std::cout << "Abyssal Glimmer - How to Play";
        rlutil::resetColor();

        int currentLine = 4;
        rlutil::locate(2, currentLine++);
        rlutil::setColor(rlutil::YELLOW);
        std::cout << "[Goal]";
        rlutil::resetColor();
        rlutil::locate(2, currentLine++);
        std::cout << "Your objective is to venture deep into the abyss, fighting through 5 floors to";
        rlutil::locate(2, currentLine++);
        std::cout << "reach the 6th and final level. Defeat the powerful Boss to win the game.";

        currentLine++;
        rlutil::locate(2, currentLine++);
        rlutil::setColor(rlutil::YELLOW);
        std::cout << "[Controls]";
        rlutil::resetColor();
        rlutil::locate(2, currentLine++);
        std::cout << "  W/A/S/D : Move your character (@). You will face the direction you move.";
        rlutil::locate(2, currentLine++);
        std::cout << "  E       : Perform a melee attack in the direction you are facing.";
        rlutil::locate(2, currentLine++);
        std::cout << "  F       : Fire a ranged projectile in the direction you are facing.";
        rlutil::locate(2, currentLine++);
        std::cout << "  ESC     : Quit the game.";
        
        currentLine++;
        rlutil::locate(2, currentLine++);
        rlutil::setColor(rlutil::YELLOW);
        std::cout << "[Gameplay Elements]";
        rlutil::resetColor();
        rlutil::locate(2, currentLine++);
        std::cout << "* HUD (Top of screen):";
        rlutil::locate(4, currentLine++);
        std::cout << "- HP     : Your health. If it reaches 0, the game is over.";
        rlutil::locate(4, currentLine++);
        std::cout << "- SHIELD : Absorbs damage before your HP. Regenerates slowly over time.";
        rlutil::locate(4, currentLine++);
        std::cout << "- LEVEL  : Your current floor in the abyss.";
        rlutil::locate(4, currentLine++);
        std::cout << "- GOLD   : Currency used to buy items and upgrades.";
        rlutil::locate(4, currentLine++);
        std::cout << "- MELEE  : Your current melee attack damage.";
        rlutil::locate(4, currentLine++);
        std::cout << "- RANGED : Your current ranged attack damage.";
        rlutil::locate(2, currentLine++);
        std::cout << "* Items (Walk over to pick up):";
        rlutil::locate(4, currentLine++);
        std::cout << "- + (Potion): Restores 10 HP.";
        rlutil::locate(4, currentLine++);
        std::cout << "- * (Coin)  : Grants 5 Gold.";
        rlutil::locate(2, currentLine++);
        std::cout << "* Enemies (Z): They appear after you move and will chase you. Defeat them for Gold.";
        rlutil::locate(2, currentLine++);
        std::cout << "* Exit (>): Find this to proceed to the next level.";

        currentLine++;
        rlutil::locate(2, currentLine++);
        rlutil::setColor(rlutil::YELLOW);
        std::cout << "[Progression]";
        rlutil::resetColor();
        rlutil::locate(2, currentLine++);
        std::cout << "- Floors 1-4: Battle monsters, gather loot, and find the exit.";
        rlutil::locate(2, currentLine++);
        std::cout << "- Floor 5 (Shop): A safe zone. Find the Merchant (M) to buy powerful upgrades.";
        rlutil::locate(2, currentLine++);
        std::cout << "- Floor 6 (Boss): The final confrontation! A multi-phase battle awaits.";
        
        currentLine++;
        rlutil::locate(2, currentLine++);
        rlutil::setColor(rlutil::YELLOW);
        std::cout << "[The Boss Fight]";
        rlutil::resetColor();
        rlutil::locate(2, currentLine++);
        std::cout << "- Phase 1: The Boss is stationary and fires a rotating barrage of projectiles.";
        rlutil::locate(2, currentLine++);
        std::cout << "- Phase 2: It transforms into a snake-like creature that chases you.";
        rlutil::locate(4, currentLine++);
        std::cout << "Only its head ('B') is vulnerable to damage!";
        rlutil::locate(4, currentLine++);
        std::cout << "Touching any part of the Boss will inflict damage.";


        rlutil::locate(2, currentLine + 2);
        std::cout << "Press any key to begin your descent...";
        rlutil::getkey(); // block until a key is pressed
        rlutil::cls();
    }

    void placeItemsAndEnemies() {
        // place several potions and coins
        for (int i = 0; i < 6; ++i) {
            placeItemRandom(IT_POTION, 10);
        }
        for (int i = 0; i < 4; ++i) {
            placeItemRandom(IT_COIN, 5);
        }
        // note: enemies are spawned after the player moves a few steps
    }

    void placeItemRandom(ItemType type, int value) {
        int rx, ry;
        int tries = 0;
        do {
            rx = std::rand() % WIDTH;
            ry = std::rand() % HEIGHT;
            tries++;
            // avoid placing on player, non-floor, or where another item or enemy already exists
            if (map[rx][ry] != FLOOR) continue;
            if (rx == player.x && ry == player.y) continue;
            if (rx == exitX && ry == exitY) continue; // Avoid placing on exit
            if (itemAt(rx, ry) != nullptr) continue;
            if (enemyAt(rx, ry) != nullptr) continue;
            break;
        } while (tries < 500);
        if (map[rx][ry] == FLOOR && itemAt(rx, ry) == nullptr && enemyAt(rx, ry) == nullptr) items.push_back({rx, ry, type, value});
    }

    // Configure level 5 as a shop level with a single merchant and no monsters
    void setupShop() {
        isShop = true;
        enemies.clear(); // no monsters
        items.clear();   // no random items; merchant sells items
        enemiesSpawned = true; // prevent delayed spawns
        movesSinceStart = 0;
        // place merchant near map center on a floor tile
        int mx = WIDTH/2; int my = HEIGHT/2;
        if (map[mx][my] != FLOOR) {
            // find nearest floor
            for (int r = 1; r < std::max(WIDTH, HEIGHT); ++r) {
                for (int dx = -r; dx <= r; ++dx) for (int dy = -r; dy <= r; ++dy) {
                    int nx = mx + dx, ny = my + dy;
                    if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT && map[nx][ny] == FLOOR) { mx = nx; my = ny; goto PLACED; }
                }
            }
        }
PLACED:
        merchant.x = mx; merchant.y = my;
        message("You find a merchant on this level. No monsters here.");
    }

    void generateBossMap() {
        // Clear map to floor
        for (int i = 0; i < WIDTH; ++i) for (int j = 0; j < HEIGHT; ++j) map[i][j] = FLOOR;
        // Outer walls
        for (int i = 0; i < WIDTH; ++i) { map[i][0] = WALL; map[i][HEIGHT-1] = WALL; }
        for (int j = 0; j < HEIGHT; ++j) { map[0][j] = WALL; map[WIDTH-1][j] = WALL; }

        // Place four 2x2 obstacles
        int qx = WIDTH / 4; int qy = HEIGHT / 4;
        // Top-left
        map[qx][qy] = WALL; map[qx+1][qy] = WALL; map[qx][qy+1] = WALL; map[qx+1][qy+1] = WALL;
        // Top-right
        map[WIDTH-qx][qy] = WALL; map[WIDTH-qx-1][qy] = WALL; map[WIDTH-qx][qy+1] = WALL; map[WIDTH-qx-1][qy+1] = WALL;
        // Bottom-left
        map[qx][HEIGHT-qy] = WALL; map[qx+1][HEIGHT-qy] = WALL; map[qx][HEIGHT-qy-1] = WALL; map[qx+1][HEIGHT-qy-1] = WALL;
        // Bottom-right
        map[WIDTH-qx][HEIGHT-qy] = WALL; map[WIDTH-qx-1][HEIGHT-qy] = WALL; map[WIDTH-qx][HEIGHT-qy-1] = WALL; map[WIDTH-qx-1][HEIGHT-qy-1] = WALL;
    }

    void setupBossLevel() {
        isBossLevel = true;
        isShop = false;
        enemies.clear(); items.clear(); projectiles.clear(); effects.clear(); bossProjectiles.clear();
        bossDefeated = false;

        generateBossMap();

        // Player starts at bottom center
        player.x = WIDTH / 2;
        player.y = HEIGHT - 3;
        
        // Boss in the middle, represented by body parts
        boss.hp = 200; // Starting HP for the boss
        boss.phase = 1;
        boss.bodyParts.clear();
        int startX = (WIDTH / 2) - 2; // for "Boss"
        for (int i = 0; i < 4; ++i) {
            boss.bodyParts.push_back({startX + i, HEIGHT / 2});
        }

        // No exit on this level
        exitX = -1; exitY = -1;

        message("The air grows cold... A powerful presence awaits!");
    }

    void openShop() {
        // Shop menu loop
        while (true) {
            draw();
            int menuY = HEIGHT + 8;
            rlutil::locate(1, menuY);
            rlutil::setColor(rlutil::LIGHTMAGENTA);
            std::cout << "--- Merchant Shop ---";
            rlutil::locate(1, menuY+1);
            std::cout << "1) Potion (10 gold) - Heals 10 HP";
            rlutil::locate(1, menuY+2);
            std::cout << "2) Full Heal (50 gold) - Restore to full HP";
            rlutil::locate(1, menuY+3);
            std::cout << "3) Ranged Upgrade (" << upgradeCost << " gold) - +2 RANGED";
            rlutil::locate(1, menuY+4);
            std::cout << "4) Ranged Potion (30 gold) - Set RANGED damage and range to 10";
            rlutil::locate(1, menuY+5);
            std::cout << "5) Melee Potion (20 gold) - Set MELEE to 20 (if lower)";
            rlutil::locate(1, menuY+6);
            std::cout << "6) Shield Flask (60 gold) - Gain +50 max SHIELD (and fill 50)";
            rlutil::locate(1, menuY+7);
            std::cout << "7) Leave Shop";
            rlutil::resetColor();
            rlutil::locate(1, menuY+9);
            std::cout << "Choose an option (1-7): ";
            int ch = rlutil::getkey();
            if (ch == '1') {
                if (player.gold >= 10) {
                    player.gold -= 10;
                    player.hp = std::min(100, player.hp + 10);
                    message("You buy a potion and recover 10 HP.");
                } else {
                    message("Not enough gold.");
                }
            } else if (ch == '2') {
                if (player.gold >= 50) {
                    player.gold -= 50;
                    player.hp = 100;
                    message("You buy a full heal and restore to 100 HP.");
                } else {
                    message("Not enough gold.");
                }
            } else if (ch == '3') {
                if (player.gold >= upgradeCost) {
                    player.gold -= upgradeCost;
                    player.rangedAtk += 2;
                    message(std::string("You pay ") + std::to_string(upgradeCost) + " gold and your RANGED increases by +2!");
                    // increase next upgrade cost
                    upgradeCost += 5;
                    prevPlayerRanged = -1;
                } else {
                    message("Not enough gold.");
                }
            } else if (ch == '4') {
                // Ranged potion -> set ranged to at least 10
                if (player.gold >= 30) {
                    player.gold -= 30;
                    bool updated = false;
                    if (player.rangedAtk < 10) {
                        player.rangedAtk = 10;
                        prevPlayerRanged = -1;
                        updated = true;
                    }
                    if (player.rangedAtkRange < 10) {
                        player.rangedAtkRange = 10;
                        updated = true;
                    }

                    if(updated) {
                        message("Your ranged attack damage and range are now 10.");
                    } else {
                        message("Your ranged attack is already 10 or higher.");
                    }
                } else {
                    message("Not enough gold.");
                }
            } else if (ch == '5') {
                // Melee potion -> set melee to at least 20
                if (player.gold >= 20) {
                    player.gold -= 20;
                    if (player.meleeAtk < 20) {
                        player.meleeAtk = 20;
                        message("Your melee attack is now 20.");
                        prevPlayerMelee = -1;
                    } else {
                        message("Your melee attack is already 20 or higher.");
                    }
                } else {
                    message("Not enough gold.");
                }
            } else if (ch == '6') {
                // Shield flask: add +50 to maxShield and restore
                if (player.gold >= 60) {
                    player.gold -= 60;
                    player.maxShield += 50;
                    player.shield = std::min(player.maxShield, player.shield + 50);
                    message("You purchase a Shield Flask: +50 SHIELD capacity and restored by 50.");
                    prevPlayerShield = -1; prevPlayerMaxShield = -1;
                } else {
                    message("Not enough gold.");
                }
            } else if (ch == '7' || ch == rlutil::KEY_ESCAPE) {
                // clear shop menu area
                for (int r = 0; r < 12; ++r) { rlutil::locate(1, menuY + r); std::cout << std::string(80, ' '); }
                draw();
                return;
            }
            // small delay so messages are readable
            draw();
            rlutil::msleep(200);
        }
    }



    void spawnDelayedEnemies(int count) {
        int minDist = 6; // minimum distance from player when spawning
        int spawned = 0;
        for (int n = 0; n < count; ++n) {
            int rx, ry; int tries = 0;
            do {
                rx = std::rand() % WIDTH;
                ry = std::rand() % HEIGHT;
                ++tries;
                int dx = rx - player.x; int dy = ry - player.y;
                if (tries > 1000) break;
                if (map[rx][ry] != FLOOR) continue;
                if (std::abs(dx) * std::abs(dx) + std::abs(dy) * std::abs(dy) < minDist*minDist) continue;
                if (enemyAt(rx, ry)) continue;
                break;
            } while (true);
            if (map[rx][ry] == FLOOR && !enemyAt(rx, ry) && (rx != player.x || ry != player.y)) {
                int hp = 20 + level * 2;
                int dmg = 10 + (int)std::round(level * 1.5);
                enemies.push_back({rx, ry, hp, dmg, ENEMY_MOVE_DELAY});
                spawned++;
            }
        }
        if (spawned > 0) {
            enemiesSpawned = true;
            message(std::string("Monsters appear: ") + std::to_string(spawned));
        }
    }

    bool isVisible(int tx, int ty) {
        // Without a light mechanic, the whole map is visible when explored
        return true;
    }

    Enemy* enemyAt(int x, int y) {
        for (auto &e : enemies) if (e.x == x && e.y == y) return &e;
        return nullptr;
    }

    Item* itemAt(int x, int y) {
        for (auto &it : items) if (it.x == x && it.y == y) return &it;
        return nullptr;
    }

    bool isBodyPart(int x, int y) {
        // Check if x, y is any of the boss's body parts.
        // We can skip the tail because it's about to move.
        for (size_t i = 0; i < boss.bodyParts.size() - 1; ++i) {
            if (boss.bodyParts[i].first == x && boss.bodyParts[i].second == y) {
                return true;
            }
        }
        return false;
    }

    void pickupAt(int x, int y) {
        for (size_t i = 0; i < items.size(); ++i) {
            if (items[i].x == x && items[i].y == y) {
                Item it = items[i];
                if (it.type == IT_POTION) {
                    player.hp = std::min(100, player.hp + it.value);
                    message("You drink the potion and recover HP.");
                } else if (it.type == IT_COIN) {
                    // Coins: flat +5 gold on pickup
                    int gain = 5;
                    addGold(gain);
                }
                items.erase(items.begin() + i);
                return;
            }
        }
    }

    // Add gold to player and ensure HUD updates and log message
    void addGold(int amount) {
        if (amount == 0) return;
        int before = player.gold;
        player.gold = std::min(GOLD_MAX, player.gold + amount);
        int actual = player.gold - before;
        if (actual > 0) {
            message(std::string("You gain ") + std::to_string(actual) + " gold.");
        } else {
            message("Your gold is already at maximum.");
        }
    }

    // Return the current actual ranged damage (used by projectiles)
    int actualRangedDamage() {
        return player.rangedAtk;
    }

    // Ranged attack in facing direction (activated by 'F')
    // Ranged attack in facing direction (activated by 'F')
    void attackFacing() {
        int dx = player.dirX;
        int dy = player.dirY;
        if (dx == 0 && dy == 0) {
            message("You fumble and fire in place.");
            return;
        }
        int sx = player.x + dx;
        int sy = player.y + dy;
        if (sx < 0 || sx >= WIDTH || sy < 0 || sy >= HEIGHT || map[sx][sy] == WALL) {
            // immediate wall in front
            message("You fire but it immediately hits something.");
            movesSinceStart++;
            if (!enemiesSpawned && movesSinceStart >= ENEMY_SPAWN_THRESHOLD) spawnDelayedEnemies(DELAYED_ENEMY_COUNT);
            return;
        }
        // spawn projectile one tile ahead
        projectiles.push_back({sx, sy, dx, dy, player.rangedAtkRange, player.rangedAtk});
        message(std::string("You fire a shot! (dmg ") + std::to_string(actualRangedDamage()) + ")");
        movesSinceStart++;
        if (!enemiesSpawned && movesSinceStart >= ENEMY_SPAWN_THRESHOLD) spawnDelayedEnemies(DELAYED_ENEMY_COUNT);
    }

    // Melee attack in facing direction (activated by 'E')
    void meleeAttack() {
        int dx = player.dirX;
        int dy = player.dirY;
        if (dx == 0 && dy == 0) {
            message("You fumble and cannot attack.");
            return;
        }

        int tx = player.x + dx;
        int ty = player.y + dy;

        // Determine glyph based on direction
        char swingGlyph = (dx != 0) ? '-' : '|';

        // Check for boss
        if (isBossLevel && boss.phase != 2 && boss.hp > 0) {
            bool hitBossPart = false;
            size_t part_idx = 0;
            for(size_t i = 0; i < boss.bodyParts.size(); ++i) {
                if (tx == boss.bodyParts[i].first && ty == boss.bodyParts[i].second) {
                    hitBossPart = true;
                    part_idx = i;
                    break;
                }
            }

            if(hitBossPart) {
                // In phase 3 (snake), only head (index 0) is vulnerable. Other phases, all parts are.
                bool vulnerable = (boss.phase != 3) || (boss.phase == 3 && part_idx == 0);
                if (vulnerable) {
                    int dmg = player.meleeAtk;
                    boss.hp -= dmg;
                    message("You strike the Boss!");
                    effects.push_back({tx, ty, swingGlyph, rlutil::LIGHTBLUE, 2});
                } else {
                    message("Your attack glances off the Boss's body.");
                    effects.push_back({tx, ty, swingGlyph, rlutil::DARKGREY, 2});
                }
                movesSinceStart++;
                return; // attack is done, don't hit other things
            }
        }

        // Check for enemy at target location for melee
        Enemy* e = enemyAt(tx, ty);
        if (e) {
            // Melee attack
            int dmg = player.meleeAtk;
            e->hp -= dmg;
            message(std::string("You slash the enemy for ") + std::to_string(dmg) + " melee damage!");
            if (e->hp <= 0) {
                message("You killed the enemy!");
                addGold(5);
            }
            e->cooldown = ENEMY_MOVE_DELAY; // Stagger enemy
            // Add visual effect
            effects.push_back({tx, ty, swingGlyph, rlutil::LIGHTBLUE, 2});
        } else {
            // No enemy to hit
            message("You swing at the empty air.");
            effects.push_back({tx, ty, swingGlyph, rlutil::LIGHTBLUE, 2});
        }

        movesSinceStart++;
        if (!enemiesSpawned && movesSinceStart >= ENEMY_SPAWN_THRESHOLD) spawnDelayedEnemies(DELAYED_ENEMY_COUNT);
    }

    void input() {
// Mouse left-click input removed; use 'F' key to attack.

        if (!kbhit()) return;
        int k = rlutil::getkey();
        int nx = player.x;
        int ny = player.y;
        if (k == rlutil::KEY_ESCAPE) { running = false; return; }

        // WSAD movement (case-insensitive)
        if (k == 'w' || k == 'W') ny--;
        else if (k == 's' || k == 'S') ny++;
        else if (k == 'a' || k == 'A') nx--;
        else if (k == 'd' || k == 'D') nx++;

        // Movement
        if (nx != player.x || ny != player.y) {
            if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT) {
                if (map[nx][ny] == WALL) {
                    message("You bumped into a wall.");
                } else {
                    if (isBossLevel && boss.hp > 0 && boss.phase == 1) {
                        bool hitBoss = false;
                        for (const auto& part : boss.bodyParts) {
                            if (nx == part.first && ny == part.second) {
                                hitBoss = true;
                                break;
                            }
                        }
                        if (hitBoss) {
                            int dmg = 20; // Damage for running into the boss
                            message("You ran into the boss and took 20 damage!");
                            int absorbed = std::min(player.shield, dmg);
                            player.shield -= absorbed;
                            dmg -= absorbed;
                            if (dmg > 0) player.hp -= dmg;
                            return; // Block movement, don't do anything else
                        }
                    }

                    Enemy* e = enemyAt(nx, ny);
                    if (e) {
                        // Enemy blocks movement; instruct to attack
                        message("Enemy blocks the way. Press 'E' for melee.");
                    } else {
                        // move: compute facing before we update position
                        int dx = nx - player.x;
                        int dy = ny - player.y;
                        if (dx != 0 || dy != 0) { player.dirX = dx; player.dirY = dy; }
                        player.x = nx; player.y = ny;

                        // pickup if any
                        pickupAt(player.x, player.y);
                        // shop interaction (if on shop level)
                        if (isShop && player.x == merchant.x && player.y == merchant.y) {
                            openShop();
                        }
                        // count movement and possibly spawn enemies
                        movesSinceStart++;
                        if (!enemiesSpawned && movesSinceStart >= ENEMY_SPAWN_THRESHOLD) {
                            spawnDelayedEnemies(DELAYED_ENEMY_COUNT);
                        }
                        // check exit
                        if (player.x == exitX && player.y == exitY) {
                            // Offer upgrades before descending
                            message("You reached the exit. Before descending, you may spend 20 gold to upgrade ranged damage.");
                            draw();
                            // prompt player to spend gold for upgrades
                            while (player.gold >= upgradeCost) {
                                rlutil::locate(1, HEIGHT + 8);
                                rlutil::setColor(rlutil::YELLOW);
                                std::cout << "Spend " << upgradeCost << " gold to increase ranged damage by +2? (Y/N) ";
                                rlutil::resetColor();
                                int ch = rlutil::getkey();
                                if (ch == 'y' || ch == 'Y') {
                                    player.gold -= upgradeCost;
                                    player.rangedAtk += 2;
                                    // increase the cost for the next upgrade
                                    upgradeCost += 5;
                                    // ensure the header will refresh and show the new actual ranged value immediately
                                    prevPlayerRanged = -1;
                                    message(std::string("You spend ") + std::to_string(upgradeCost - 5) + " gold to upgrade your ranged attack (+2)!");
                                    draw();
                                } else if (ch == 'n' || ch == 'N') {
                                    break;
                                }
                            }

                            level++;
                            message("You descend to the next level.");
                            // reset and generate next level
                            enemies.clear(); items.clear();
                            init();
                            return;
                        }
                    }
                }
            }
            return;
        }

        // Attack handling: F for ranged, E for melee
        if (k == 'f' || k == 'F') {
            attackFacing();
        } else if (k == 'e' || k == 'E') {
            meleeAttack();
        }
    }

    void updateEnemies() {
        // Simple enemy action: if player is seen, move towards them, otherwise move randomly
        for (auto &e : enemies) {
            // respect cooldown to slow enemy actions
            if (e.cooldown > 0) { e.cooldown--; continue; }

            int dx = player.x - e.x;
            int dy = player.y - e.y;
            if (dx*dx + dy*dy <= (ENEMY_SIGHT_RADIUS*ENEMY_SIGHT_RADIUS)) {
                int sx = (dx > 0) ? 1 : (dx < 0) ? -1 : 0;
                int sy = (dy > 0) ? 1 : (dy < 0) ? -1 : 0;
                int nx = e.x + sx;
                int ny = e.y + sy;
                if (nx == player.x && ny == player.y) {
                    // attack player; shield absorbs damage first
                    int dmg = e.dmg;
                    int absorbed = std::min(player.shield, dmg);
                    player.shield -= absorbed;
                    dmg -= absorbed;
                    if (dmg > 0) {
                        player.hp -= dmg;
                        if (absorbed > 0) message(std::string("The enemy hits your shield for ") + std::to_string(absorbed) + " and your HP for " + std::to_string(dmg) + "!");
                        else message(std::string("The enemy hits you for ") + std::to_string(dmg) + "!");
                    } else {
                        message(std::string("The enemy hits your shield for ") + std::to_string(absorbed) + "!");
                    }
                    e.cooldown = ENEMY_MOVE_DELAY;
                } else if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT && map[nx][ny] == FLOOR && !enemyAt(nx, ny)) {
                    e.x = nx; e.y = ny;
                    e.cooldown = ENEMY_MOVE_DELAY;
                }
            } else {
                // Random movement
                int dir = std::rand() % 4;
                int nx = e.x, ny = e.y;
                if (dir == 0 && e.x + 1 < WIDTH) nx++;
                if (dir == 1 && e.x - 1 >= 0) nx--;
                if (dir == 2 && e.y + 1 < HEIGHT) ny++;
                if (dir == 3 && e.y - 1 >= 0) ny--;
                if (map[nx][ny] == FLOOR && !enemyAt(nx, ny) && (nx != player.x || ny != player.y)) {
                    e.x = nx; e.y = ny;
                    e.cooldown = ENEMY_MOVE_DELAY;
                }
            }
        }
    }

    // find projectile at tile
    Projectile* projectileAt(int x, int y) {
        for (auto &p : projectiles) if (p.x == x && p.y == y) return &p;
        return nullptr;
    }

    void updateProjectiles() {
        for (size_t i = 0; i < projectiles.size(); ) {
            Projectile &p = projectiles[i];
            int nx = p.x + p.dx;
            int ny = p.y + p.dy;
            // out of bounds
            if (nx < 0 || nx >= WIDTH || ny < 0 || ny >= HEIGHT) {
                projectiles.erase(projectiles.begin() + i);
                continue;
            }
            // hit wall
            if (map[nx][ny] == WALL) {
                message("Your shot hits a wall.");
                projectiles.erase(projectiles.begin() + i);
                continue;
            }
            // move
            p.x = nx; p.y = ny;
            // hit boss?
            if (isBossLevel && boss.phase != 2 && boss.hp > 0) {
                bool hitBossPart = false;
                size_t part_idx = 0;
                for(size_t k = 0; k < boss.bodyParts.size(); ++k) {
                    if (p.x == boss.bodyParts[k].first && p.y == boss.bodyParts[k].second) {
                        hitBossPart = true;
                        part_idx = k;
                        break;
                    }
                }

                if(hitBossPart) {
                    // In phase 3 (snake), only head (index 0) is vulnerable. Other phases, all parts are.
                    bool vulnerable = (boss.phase != 3) || (boss.phase == 3 && part_idx == 0);
                    if (vulnerable) {
                        boss.hp -= p.dmg;
                        message(std::string("Your shot hits the Boss for ") + std::to_string(p.dmg) + " damage!");
                    } else {
                        message("Your shot glances off the Boss's body.");
                    }
                    projectiles.erase(projectiles.begin() + i);
                    continue;
                }
            }
            // hit enemy?
            Enemy* e = enemyAt(p.x, p.y);
            if (e) {
                e->hp -= p.dmg;
                message(std::string("Your shot hits an enemy for ") + std::to_string(p.dmg) + " damage!");
                if (e->hp <= 0) {
                    message("You slay the enemy from range!");
                    addGold(5);
                }
                e->cooldown = ENEMY_MOVE_DELAY;
                projectiles.erase(projectiles.begin() + i);
                continue;
            }
            // decrement range
            p.range--;
            if (p.range <= 0) {
                projectiles.erase(projectiles.begin() + i);
                continue;
            }
            ++i;
        }
    }

    void updateBoss() {
        if (!isBossLevel || boss.hp <= 0) return;

        // Phase 1 -> 2 (Transition)
        if (boss.phase == 1 && boss.hp <= 100) {
            boss.phase = 2;
            transitionCounter = 0;
            bossProjectiles.clear();
            message("The Boss roars and begins to transform!");
            return;
        }
        // Phase 2 -> 3 (Real Phase 2)
        else if (boss.phase == 2 && transitionCounter > 83) {
            boss.phase = 3;
            boss.bodyParts.clear();
            const int snakeLength = 13;
            int startX = (WIDTH / 2) - (snakeLength / 2);
            for (int i = 0; i < snakeLength; ++i) {
                boss.bodyParts.push_back({startX + i, HEIGHT / 2});
            }
            message("The transformation is complete!");
        }
        // Phase 3 -> End? (User will specify)
        else if (boss.phase == 3 && boss.hp <= 300) {
            // For now, do nothing. Await user instructions for next phase.
        }


        // --- Behavior ---
        if (boss.phase == 1) {
            // Sweeping, rotating attack
            bossAttackCounter++;
            if (bossAttackCounter >= 4) {
                bossAttackCounter = 0;
                int dirs[8][2] = {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}};
                int dx = dirs[bossAttackDirection][0];
                int dy = dirs[bossAttackDirection][1];
                // Fire from center of the 4-part body
                bossProjectiles.push_back({boss.bodyParts[2].first, boss.bodyParts[2].second, dx, dy, 20, 20});
                bossAttackDirection = (bossAttackDirection + 1) % 8;
            }
        }
        else if (boss.phase == 2) {
            // Flickering, waiting, and growing
            transitionCounter++;
            if (transitionCounter > 0 && transitionCounter % 9 == 0 && boss.bodyParts.size() < 13) {
                std::pair<int, int> tail = boss.bodyParts.back();
                boss.bodyParts.push_back({tail.first + 1, tail.second});
            }
        }
        else if (boss.phase == 3) {
            // Snake-like movement and attack
            bossMoveCooldown++;
            if (bossMoveCooldown > 2) {
                bossMoveCooldown = 0;

                std::pair<int, int> head = boss.bodyParts.front();
                int dx = player.x - head.first;
                int dy = player.y - head.second;

                int sx = (dx > 0) ? 1 : (dx < 0) ? -1 : 0;
                int sy = (dy > 0) ? 1 : (dy < 0) ? -1 : 0;

                int primary_nx = head.first;
                int primary_ny = head.second;
                int secondary_nx = head.first;
                int secondary_ny = head.second;

                // Determine primary and secondary axes of movement
                if (std::abs(dx) > std::abs(dy)) {
                    primary_nx += sx;   // Primary is horizontal
                    secondary_ny += sy; // Secondary is vertical
                } else {
                    primary_ny += sy;   // Primary is vertical
                    secondary_nx += sx; // Secondary is horizontal
                }

                int final_nx = -1, final_ny = -1;

                // Lambda to check if a move to (x, y) is valid
                auto is_valid_move = [&](int x, int y) {
                    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return false;
                    if (map[x][y] == WALL) return false;
                    if (isBodyPart(x, y)) return false;
                    return true;
                };
                
                // 1. Try primary direction
                if ((primary_nx == player.x && primary_ny == player.y) || is_valid_move(primary_nx, primary_ny)) {
                    final_nx = primary_nx;
                    final_ny = primary_ny;
                } 
                // 2. Try secondary direction
                else if ((secondary_nx == player.x && secondary_ny == player.y) || is_valid_move(secondary_nx, secondary_ny)) {
                    final_nx = secondary_nx;
                    final_ny = secondary_ny;
                }
                // 3. Both are blocked, try to un-stick by checking cardinal directions
                else {
                    int moves[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
                    for(int i = 0; i < 4; ++i) {
                        int check_nx = head.first + moves[i][0];
                        int check_ny = head.second + moves[i][1];
                        if (is_valid_move(check_nx, check_ny)) {
                            final_nx = check_nx;
                            final_ny = check_ny;
                            break;
                        }
                    }
                }

                // If we found a valid move
                if (final_nx != -1) {
                    // Check for collision with player at the final destination
                    if (final_nx == player.x && final_ny == player.y) {
                        int dmg = 20;
                        int absorbed = std::min(player.shield, dmg);
                        player.shield -= absorbed;
                        dmg -= absorbed;
                        if (dmg > 0) player.hp -= dmg;
                        message("The Boss slams its head into you!");
                        // Don't move body, just attack
                    } else {
                        // Move the boss
                        boss.bodyParts.insert(boss.bodyParts.begin(), {final_nx, final_ny});
                        boss.bodyParts.pop_back();
                    }
                }
                // else, the boss is truly trapped and does not move this turn.
            }
        }
    }

    void updateBossProjectiles() {
        for (size_t i = 0; i < bossProjectiles.size(); ) {
            BossProjectile &p = bossProjectiles[i];
            int nx = p.x + p.dx;
            int ny = p.y + p.dy;

            // Out of bounds or hit wall
            if (nx < 0 || nx >= WIDTH || ny < 0 || ny >= HEIGHT || map[nx][ny] == WALL) {
                bossProjectiles.erase(bossProjectiles.begin() + i);
                continue;
            }

            // Move
            p.x = nx; p.y = ny;

            // Hit player?
            if (p.x == player.x && p.y == player.y) {
                int dmg = p.dmg;
                int absorbed = std::min(player.shield, dmg);
                player.shield -= absorbed;
                dmg -= absorbed;
                if (dmg > 0) player.hp -= dmg;
                message("You are hit by an energy ball!");
                bossProjectiles.erase(bossProjectiles.begin() + i);
                continue;
            }

            // Decrement range
            p.range--;
            if (p.range <= 0) {
                bossProjectiles.erase(bossProjectiles.begin() + i);
                continue;
            }
            ++i;
        }
    }

    void updatePlayerBossOverlap() {
        if (playerOnBossCooldown > 0) playerOnBossCooldown--;
        if (!isBossLevel || boss.phase != 3) return;

        bool onBoss = false;
        for (const auto& part : boss.bodyParts) {
            if (player.x == part.first && player.y == part.second) {
                onBoss = true;
                break;
            }
        }

        if (onBoss && playerOnBossCooldown == 0) {
            message("You are burned by the boss's body!");
            int dmg = 20;
            int absorbed = std::min(player.shield, dmg);
            player.shield -= absorbed;
            dmg -= absorbed;
            if (dmg > 0) player.hp -= dmg;
            playerOnBossCooldown = 8; // Approx 0.5 seconds
        }
    }

    void updateEffects() {
        for (size_t i = 0; i < effects.size(); ) {
            effects[i].lifetime--;
            if (effects[i].lifetime <= 0) {
                effects.erase(effects.begin() + i);
            } else {
                ++i;
            }
        }
    }

    void drawVictoryScreen() {
        rlutil::cls();
        rlutil::setColor(rlutil::YELLOW);
        const char* win_art[] = {
            "Y   Y  OOO  U   U       W   W  III  N   N",
            " Y Y  O   O U   U       W   W   I   NN  N",
            "  Y   O   O U   U       W W W   I   N N N",
            "  Y   O   O U   U       W W W   I   N  NN",
            "  Y    OOO   UUU         W W   III  N   N"
        };
        int startY = (HEIGHT / 2) - 4;
        for(int i = 0; i < 5; ++i) {
            rlutil::locate(1, startY + i);
            std::cout << win_art[i];
        }

        std::string msg = "ESC to exit, R to restart";
        rlutil::locate((WIDTH - msg.length()) / 2, startY + 7);
        std::cout << msg;
        rlutil::resetColor();
    }



    void draw() {
        // Update top status bar (only redraw the whole line if a value changes)
        if (player.hp != prevPlayerHP || player.gold != prevPlayerGold || level != prevLevel || player.meleeAtk != prevPlayerMelee || player.rangedAtk != prevPlayerRanged || player.shield != prevPlayerShield || player.maxShield != prevPlayerMaxShield) {
            rlutil::locate(1,1);
            rlutil::setColor(rlutil::LIGHTGREEN);
            // build a padded header line to avoid leftover digits when values shrink
            std::ostringstream hdr;
            hdr << "HP: " << player.hp << "  ";
            hdr << "LEVEL: " << level << "  ";
            hdr << "GOLD: " << player.gold << "  ";
            hdr << "MELEE: " << player.meleeAtk << "  ";
            hdr << "RANGED: " << player.rangedAtk << "  ";
            hdr << "SHIELD: " << player.shield << "/" << player.maxShield;
            std::string hdrs = hdr.str();
            // pad to a fixed minimum width to clear previous content
            if (hdrs.size() < 80) hdrs += std::string(80 - hdrs.size(), ' ');
            std::cout << hdrs;
            rlutil::resetColor();

            prevPlayerHP = player.hp;
            prevPlayerGold = player.gold;
            prevLevel = level;
            prevPlayerMelee = player.meleeAtk;
            prevPlayerRanged = player.rangedAtk;
            prevPlayerShield = player.shield;
            prevPlayerMaxShield = player.maxShield;
        }

        // Map area: only update changed cells to avoid full-screen flicker
        for (int j = 0; j < HEIGHT; ++j) {
            for (int i = 0; i < WIDTH; ++i) {
                bool vis = isVisible(i, j);
                if (vis) visited[i][j] = true;

                char glyph = ' ';
                int color = -1;

                if (!vis && !visited[i][j]) {
                    glyph = ' ';
                    color = -1;
                } else if (!vis && visited[i][j]) {
                    glyph = (map[i][j] == WALL) ? '#' : '.';
                    color = rlutil::DARKGREY;
                } else {
                    // visible
                    if (i == player.x && j == player.y) {
                        glyph = '@'; color = rlutil::YELLOW;
                    } else {
                        // projectile?
                        Projectile* pr = projectileAt(i, j);
                        if (pr) { glyph = 'o'; color = rlutil::LIGHTBLUE; }
                        else {
                            BossProjectile* bp = nullptr;
                            for(auto &bprj : bossProjectiles) if(bprj.x == i && bprj.y == j) { bp = &bprj; break; }
                            if (bp) { glyph = 'O'; color = rlutil::MAGENTA; }
                            else {
                                // item?
                                Item* it = nullptr;
                                for (auto &itm : items) if (itm.x == i && itm.y == j) { it = &itm; break; }
                                if (it) {
                                    glyph = (it->type == IT_POTION) ? '+' : '*';
                                    color = (it->type == IT_POTION) ? rlutil::LIGHTRED : rlutil::YELLOW;
                                } else {
                                    Enemy* en = enemyAt(i,j);
                                    if (en) { glyph = 'Z'; color = rlutil::RED; }
                                    else if (isShop && i == merchant.x && j == merchant.y) { glyph = 'M'; color = rlutil::MAGENTA; }
                                    else if (i == exitX && j == exitY) { glyph = '>'; color = rlutil::CYAN; }
                                    else { glyph = (map[i][j] == WALL) ? '#' : '.'; color = -1; }
                                }
                            }
                        }
                    }
                }

                // Draw boss on top of anything else
                if (isBossLevel && boss.hp > 0) {
                    bool shouldDrawBoss = !(boss.phase == 2 && transitionCounter % 2 != 0);
                    if (shouldDrawBoss) {
                        for (size_t k = 0; k < boss.bodyParts.size(); ++k) {
                            if (boss.bodyParts[k].first == i && boss.bodyParts[k].second == j) {
                                if (k == 0) glyph = 'B';
                                else if (k == 1) glyph = 'o';
                                else glyph = 's';
                                color = rlutil::RED;
                                break;
                            }
                        }
                    }
                }
                
                // Draw effects on top
                for (const auto& effect : effects) {
                    if (effect.x == i && effect.y == j) {
                        glyph = effect.glyph;
                        color = effect.color;
                    }
                }

                // Only redraw if different from the previous frame
                if (prevGlyph[i][j] != glyph || prevColor[i][j] != color) {
                    int drawX = i + 1;
                    int drawY = j + 2; // Leave space for status bar
                    rlutil::locate(drawX, drawY);
                    if (color != -1) rlutil::setColor(color);
                    std::cout << glyph;
                    if (color != -1) rlutil::resetColor();
                    prevGlyph[i][j] = glyph;
                    prevColor[i][j] = color;
                }
            }
        }

        // Message area (bottom): if content changes, clear and redraw
        if (logLines != prevLogLines) {
            int msgY = HEIGHT + 3;
            // Clear the three-line area
            for (int r = 0; r < 3; ++r) {
                rlutil::locate(1, msgY + r);
                std::cout << std::string(WIDTH + 10, ' ');
            }
            rlutil::setColor(rlutil::LIGHTCYAN);
            for (size_t i = 0; i < logLines.size(); ++i) {
                rlutil::locate(1, msgY + (int)i);
                std::cout << logLines[i];
            }
            rlutil::resetColor();
            prevLogLines = logLines;
        }

        // fixed prompt (rarely changes)
        rlutil::locate(1, HEIGHT + 6);
        std::cout << "(ESC to quit)";
    }

    void restartGame() {
        rlutil::cls();
        // Reset to new-game defaults
        message("Restarting game...");
        level = 1;
        running = true;
        player.gold = 0;
        player.meleeAtk = 8;
        player.rangedAtk = 8;
        player.rangedAtkRange = 6;
        player.hp = 100;
        player.shield = 0;
        player.maxShield = 0;
        upgradeCost = 20;
        isShop = false;
        enemies.clear(); items.clear(); projectiles.clear(); effects.clear(); bossProjectiles.clear();
        enemiesSpawned = false; movesSinceStart = 0;
        prevLogLines.clear();
        prevPlayerHP = prevPlayerGold = prevLevel = -1;
        prevPlayerMelee = prevPlayerRanged = prevPlayerShield = prevPlayerMaxShield = -1;
        // don't show intro on restart
        introShown = true;
        init();
        draw();
    }

    void run() {
        // Show instructions once at start
        if (!introShown) {
            showInstructions();
            introShown = true;
        }

        rlutil::hidecursor();
        while (running) {
            input();
            if (!running) break;
            updateProjectiles();
            updateBossProjectiles();
            updateEnemies();
            updateBoss();
            updatePlayerBossOverlap();
            updateEffects();

            // Shield regeneration over time
            if (player.maxShield > 0 && player.shield < player.maxShield) {
                shieldRegenCounter++;
                if (shieldRegenCounter >= SHIELD_REGEN_INTERVAL) {
                    player.shield = std::min(player.maxShield, player.shield + SHIELD_REGEN_AMOUNT);
                    shieldRegenCounter = 0;
                }
            }

            draw();

            // remove dead enemies (safe)
            enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](const Enemy &e){ return e.hp <= 0; }), enemies.end());

            // check boss defeat / win condition
            if (isBossLevel && boss.hp <= 0 && !bossDefeated) {
                bossDefeated = true;
                
                drawVictoryScreen();

                // Wait for quit or restart
                bool awaiting = true;
                while (awaiting) {
                    if (kbhit()) {
                        int k = rlutil::getkey();
                        if (k == 'r' || k == 'R') {
                            restartGame();
                            awaiting = false;
                        } else if (k == rlutil::KEY_ESCAPE) {
                            running = false;
                            awaiting = false;
                        }
                    }
                    rlutil::msleep(100);
                }
                if (!running) break;
                continue;
            }

            // check death
            if (player.hp <= 0) {
                message("You have died. Game over. Press 'R' to restart or ESC to quit.");
                draw();
                // wait for restart or quit
                bool awaiting = true;
                while (awaiting) {
                    if (kbhit()) {
                        int k = rlutil::getkey();
                        if (k == 'r' || k == 'R') {
                            restartGame();
                            awaiting = false;
                        } else if (k == rlutil::KEY_ESCAPE) {
                            running = false;
                            awaiting = false;
                        }
                    }
                    rlutil::msleep(100);
                }
                if (!running) break;
                continue; // continue main loop with restarted state
            }

            rlutil::msleep(60);
        }
        rlutil::showcursor();
    }
};

int main() {
#ifdef _WIN32
    // Enable UTF-8 input and output (Windows)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    // Set C/C++ locale to system default (so printf/iostream use correct wide chars and encoding)
    std::setlocale(LC_ALL, "");

    Game game;
    game.run();
    return 0;
}