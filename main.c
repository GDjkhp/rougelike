#include "raylib.h"
#include "raymath.h"

typedef struct {
    Vector3 position;
    Vector3 size;
    float speed;
    Texture2D texture;
    int direction;  // 0: down, 1: right, 2: up, 3: left
    
    // New physics parameters
    Vector3 velocity;
    bool isGrounded;
    float jumpForce;
    float gravity;
    
    // Health system
    int health;
    bool isHit;
    float hitTimer;
} Player;

// New Enemy struct
typedef struct {
    Vector3 position;
    Vector3 size;
    float speed;
    Texture2D texture;
    
    // Enemy behavior
    float shootTimer;
    float shootCooldown;
    bool active;
    
    // Health system
    int health;
    bool isHit;
    float hitTimer;
} Enemy;

// New Bullet struct
typedef struct {
    Vector3 position;
    Vector3 direction;
    float speed;
    float radius;
    Color color;
    bool active;
    bool fromPlayer;  // Flag to determine if bullet is from player or enemy
} Bullet;

#define MAX_ENEMIES 100
#define MAX_BULLETS 500
#define ENEMY_SHOOT_COOLDOWN 2.0f
#define BULLET_SPEED 0.3f
#define BULLET_RADIUS 0.15f

typedef enum {
    GAME_PLAYING,
    GAME_PAUSED
} GameState;

bool CheckCollisionPlayerWithMap(Player *player, Model model, Vector3 mapPosition, Texture2D cubicmap);
void UpdateGameCamera(Camera *camera, Player player);
BoundingBox GetPlayerBoundingBox(Player player);
void UpdatePlayerPhysics(Player *player, float deltaTime, Model model, Vector3 mapPosition, Texture2D cubicmap);
bool CheckCollisionSphereBox(Vector3 center, float radius, BoundingBox box);
BoundingBox GetEnemyBoundingBox(Enemy enemy);
void UpdateEnemies(Enemy *enemies, int enemyCount, Player player, Bullet *bullets, int *bulletCount, float deltaTime, Model model, Vector3 mapPosition, Texture2D cubicmap);
void ShootBullet(Bullet *bullets, int *bulletCount, Vector3 position, Vector3 direction, bool fromPlayer);
void UpdateBullets(Bullet *bullets, int bulletCount, float deltaTime);
bool CheckCollisionBulletWithMap(Bullet bullet, Model model, Vector3 mapPosition, Texture2D cubicmap);
void CheckBulletCollisions(Bullet *bullets, int bulletCount, Player *player, Enemy *enemies, int enemyCount, Model model, Vector3 mapPosition, Texture2D cubicmap);

int main(void)
{
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "roguelike");

    Camera camera = { 0 };
    camera.position = (Vector3){ 16.0f, 18.0f, 16.0f };     // Camera position
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };          // Camera looking at point
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };              // Camera up vector (rotation towards target)
    camera.fovy = 45.0f;                                    // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;                 // Camera projection type

    Player player = {0};
    player.position = (Vector3){ 0.0f, 0.5f, -2.0f };
    // Make player smaller to fit in 1-cube spaces
    player.size = (Vector3){ 0.5f, 0.5f, 0.5f };
    player.speed = 0.25f;
    player.texture = LoadTexture("resources/player.png");
    player.direction = 0;
    
    // Initialize physics parameters
    player.velocity = (Vector3){ 0.0f, 0.0f, 0.0f };
    player.isGrounded = true;
    player.jumpForce = 0.2f;
    player.gravity = 0.01f;
    
    // Initialize player health
    player.health = 100;
    player.isHit = false;
    player.hitTimer = 0.0f;

    // Initialize enemies
    Enemy enemies[MAX_ENEMIES] = {0};
    int enemyCount = 10; // Start with 10 enemies
    
    Texture2D enemyTexture = LoadTexture("resources/enemy.png");
    if (!enemyTexture.id) {
        // If enemy texture not found, use a default color instead
        enemyTexture = player.texture; // Fallback to player texture
    }
    
    // Initialize enemy positions randomly
    for (int i = 0; i < enemyCount; i++) {
        enemies[i].position = (Vector3){
            GetRandomValue(0, 10) + 0.5f,
            0.5f,
            GetRandomValue(0, 10) + 0.5f
        };
        
        enemies[i].size = (Vector3){ 0.5f, 0.5f, 0.5f };
        enemies[i].speed = 0.13f; // Slower than player
        enemies[i].texture = enemyTexture;
        enemies[i].shootTimer = GetRandomValue(0, 100) / 100.0f * ENEMY_SHOOT_COOLDOWN; // Randomize initial shoot timer
        enemies[i].shootCooldown = ENEMY_SHOOT_COOLDOWN;
        enemies[i].active = true;
        enemies[i].health = 30;
        enemies[i].isHit = false;
        enemies[i].hitTimer = 0.0f;
    }
    
    // Initialize bullets
    Bullet bullets[MAX_BULLETS] = {0};
    int bulletCount = 0;

    Image image = LoadImage("resources/map.png");           // Load map image (RAM)
    Texture2D cubicmap = LoadTextureFromImage(image);       // Convert image to texture to display (VRAM)

    Mesh mesh = GenMeshCubicmap(image, (Vector3){ 1.0f, 1.0f, 1.0f });
    Model model = LoadModelFromMesh(mesh);

    Texture2D texture = LoadTexture("resources/cubicmap_atlas.png");
    model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;

    // Map dimensions (used for minimap calculations)
    int mapWidth = image.width;
    int mapHeight = image.height;
    
    Vector3 mapPosition = { -16.0f, 0.0f, -8.0f };          // Set model position

    UnloadImage(image);     // Unload map image from RAM, already uploaded to VRAM

    GameState gameState = GAME_PLAYING;
    
    bool showWireframe = true;
    
    // Player shooting variables
    bool canShoot = true;
    float shootTimer = 0.0f;
    float shootCooldown = 0.5f;

    SetTargetFPS(60);

    while (!WindowShouldClose())        // Detect window close button or ESC key
    {
        // Get frame time to ensure consistent physics regardless of framerate
        float deltaTime = GetFrameTime();
        
        if (IsKeyPressed(KEY_P)) 
        {
            if (gameState == GAME_PLAYING) gameState = GAME_PAUSED;
            else if (gameState == GAME_PAUSED) gameState = GAME_PLAYING;
        }
        
        // Toggle wireframe display
        if (IsKeyPressed(KEY_F))
        {
            showWireframe = !showWireframe;
        }

        if (gameState == GAME_PLAYING)
        {
            // Update hit timer for player
            if (player.isHit) {
                player.hitTimer -= deltaTime;
                if (player.hitTimer <= 0.0f) {
                    player.isHit = false;
                }
            }
        
            // Store previous position
            Vector3 previousPosition = player.position;

            // Player horizontal movement (no vertical movement here)
            if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) 
            {
                player.position.x += player.speed;
                player.direction = 1;
            }
            else if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) 
            {
                player.position.x -= player.speed;
                player.direction = 3;
            }
            
            if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) 
            {
                player.position.z += player.speed;
                player.direction = 0;
            }
            else if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) 
            {
                player.position.z -= player.speed;
                player.direction = 2;
            }
            
            // Check horizontal movement collisions
            if (CheckCollisionPlayerWithMap(&player, model, mapPosition, cubicmap))
            {
                player.position = previousPosition;  // Restore position if collision detected
            }
            
            // Handle jumping - can always jump regardless of space constraints
            if (IsKeyPressed(KEY_SPACE) && player.isGrounded)
            {
                player.velocity.y = player.jumpForce;
                player.isGrounded = false;
            }
            
            // Player shooting
            if (!canShoot) {
                shootTimer -= deltaTime;
                if (shootTimer <= 0.0f) {
                    canShoot = true;
                }
            }
            
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && canShoot) {
                // Get mouse position in 3D world space
                Ray ray = GetMouseRay(GetMousePosition(), camera);
                
                // Project the mouse ray onto the same y-plane as the player + 0.5f
                float t = ((player.position.y + 0.5f) - ray.position.y) / ray.direction.y;
                Vector3 targetPoint = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
                
                // Calculate the direction vector from player to the projected point
                Vector3 direction = Vector3Normalize(Vector3Subtract(
                    targetPoint,
                    (Vector3){ player.position.x, player.position.y + 0.5f, player.position.z }));
                
                // Force the y component to be zero to ensure horizontal shooting
                direction.y = 0.0f;
                
                // Re-normalize after zeroing the y component
                direction = Vector3Normalize(direction);
                
                // Shoot bullet
                ShootBullet(bullets, &bulletCount, 
                    (Vector3){ player.position.x, player.position.y + 0.5f, player.position.z }, 
                    direction, true);
                
                canShoot = false;
                shootTimer = shootCooldown;
            }
            
            // Update physics (vertical movement, gravity, ground detection)
            UpdatePlayerPhysics(&player, deltaTime, model, mapPosition, cubicmap);
            
            // Update enemies
            UpdateEnemies(enemies, enemyCount, player, bullets, &bulletCount, deltaTime, model, mapPosition, cubicmap);
            
            // Update bullets
            UpdateBullets(bullets, bulletCount, deltaTime);
            
            // Check bullet collisions
            CheckBulletCollisions(bullets, bulletCount, &player, enemies, enemyCount, model, mapPosition, cubicmap);

            // Update camera to follow player
            UpdateGameCamera(&camera, player);
            
            // Check game over condition
            if (player.health <= 0) {
                // You could add game over state here
                gameState = GAME_PAUSED;
            }
        }

        // Camera zoom control
        float mouseWheel = GetMouseWheelMove();
        if (mouseWheel != 0)
        {
            // Adjust camera height (zoom)
            camera.position.y -= mouseWheel * 2.0f;
            if (camera.position.y < 5.0f) camera.position.y = 5.0f;
            if (camera.position.y > 30.0f) camera.position.y = 30.0f;
        }

        BeginDrawing();

            ClearBackground(BLACK);

            BeginMode3D(camera);

                // Draw the 3D map
                DrawModel(model, mapPosition, 1.0f, WHITE);

                // Draw wireframes for collision cubes
                if (showWireframe)
                {
                    // Load the cubicmap image to check which cubes should have wireframes
                    Image cubicmapImage = LoadImageFromTexture(cubicmap);
                    
                    // Loop through all cells in the map
                    for (int z = 0; z < cubicmap.height; z++)
                    {
                        for (int x = 0; x < cubicmap.width; x++)
                        {
                            // Get pixel color from cubicmap
                            Color pixelColor = GetImageColor(cubicmapImage, x, z);
                            
                            // If the pixel is white (or close to white) - draw a wireframe cube
                            if ((pixelColor.r > 200) && (pixelColor.g > 200) && (pixelColor.b > 200))
                            {
                                // Calculate the position of the cube in the 3D world
                                Vector3 cubePosition = {
                                    mapPosition.x + x, // Center of the cube
                                    mapPosition.y + 0.5f,     // Center of the cube
                                    mapPosition.z + z  // Center of the cube
                                };
                                
                                // Draw wireframe cube
                                DrawCubeWires(cubePosition, 1.0f, 1.0f, 1.0f, BLUE);
                            }
                        }
                    }
                    
                    // Unload the image data
                    UnloadImage(cubicmapImage);
                }

                // Draw the 2D player as billboard with hit effect
                Color playerColor = player.isHit ? RED : WHITE;
                DrawBillboard(camera, player.texture, 
                    (Vector3){ player.position.x, player.position.y + 0.5f, player.position.z }, 
                    1.0f, playerColor);
                
                // Draw enemies
                for (int i = 0; i < enemyCount; i++) {
                    if (enemies[i].active) {
                        Color enemyColor = enemies[i].isHit ? RED : WHITE;
                        DrawBillboard(camera, enemies[i].texture, 
                            (Vector3){ enemies[i].position.x, enemies[i].position.y + 0.5f, enemies[i].position.z }, 
                            1.0f, enemyColor);
                            
                        // Draw enemy health bar
                        Vector3 healthBarPos = (Vector3){ 
                            enemies[i].position.x, 
                            enemies[i].position.y + 1.0f, 
                            enemies[i].position.z 
                        };
                        
                        float healthPercent = (float)enemies[i].health / 30.0f;
                        DrawCube(
                            Vector3Subtract(healthBarPos, (Vector3){ (1.0f - healthPercent) * 0.25f, 0, 0 }), 
                            healthPercent * 0.5f, 0.1f, 0.1f, 
                            (Color){ 255, (unsigned char)(healthPercent * 255), 0, 255 }
                        );
                    }
                }
                
                // Draw bullets
                for (int i = 0; i < bulletCount; i++) {
                    if (bullets[i].active) {
                        DrawSphere(bullets[i].position, bullets[i].radius, bullets[i].color);
                    }
                }
                
                // Draw player's bounding box if enabled
                if (showWireframe)
                {
                    BoundingBox mapBounds = GetModelBoundingBox(model);
                    mapBounds.min = Vector3Add(mapBounds.min, mapPosition);
                    mapBounds.max = Vector3Add(mapBounds.max, mapPosition);
                    DrawBoundingBox(mapBounds, GREEN);

                    BoundingBox playerBox = GetPlayerBoundingBox(player);
                    DrawBoundingBox(playerBox, RED);
                    
                    // Draw enemy bounding boxes
                    for (int i = 0; i < enemyCount; i++) {
                        if (enemies[i].active) {
                            BoundingBox enemyBox = GetEnemyBoundingBox(enemies[i]);
                            DrawBoundingBox(enemyBox, PURPLE);
                        }
                    }
                    
                    // Draw lines from player to surrounding cells to help visualize
                    Vector3 playerCenter = {
                        player.position.x,
                        player.position.y + 0.5f,
                        player.position.z
                    };

                    // Draw debug grid lines around player
                    for (int z = -1; z <= 1; z++)
                    {
                        for (int x = -1; x <= 1; x++)
                        {
                            int currentCellX = (int)(player.position.x - mapPosition.x) + x;
                            int currentCellZ = (int)(player.position.z - mapPosition.z) + z;
                            
                            // Skip cells outside the map bounds
                            if (currentCellX < 0 || currentCellZ < 0 || 
                                currentCellX >= cubicmap.width || currentCellZ >= cubicmap.height)
                                continue;

                            // Create a box for the current wall cell
                            float cellXOffset = currentCellX - 0.5f;
                            float cellZOffset = currentCellZ - 0.5f;
                            BoundingBox cellBounds = {
                                (Vector3){ mapPosition.x + cellXOffset, mapPosition.y, mapPosition.z + cellZOffset },
                                (Vector3){ mapPosition.x + cellXOffset + 1.0f, mapPosition.y + 1.0f, mapPosition.z + cellZOffset + 1.0f }
                            };
                            DrawBoundingBox(cellBounds, GREEN);
                            
                            // Calculate center of cell
                            Vector3 cellCenter = {
                                mapPosition.x + cellXOffset + 0.5f,
                                player.position.y,
                                mapPosition.z + cellZOffset + 0.5f
                            };
                            
                            // Draw line from player to cell center
                            DrawLine3D(playerCenter, cellCenter, GREEN);

                            
                        }
                    }
                }

            EndMode3D();

            // Draw minimap
            DrawTextureEx(cubicmap, (Vector2){ screenWidth - cubicmap.width*4.0f - 20, 20.0f }, 0.0f, 4.0f, WHITE);
            DrawRectangleLines(screenWidth - cubicmap.width*4 - 20, 20, cubicmap.width*4, cubicmap.height*4, GREEN);
            
            // Draw player position on minimap - FIXED CALCULATION
            float minimapScale = 4.0f;
            int minimapX = screenWidth - cubicmap.width*minimapScale - 20;
            int minimapY = 20;
            
            // Calculate the normalized position of the player within the map bounds
            float normalizedX = (player.position.x - mapPosition.x) / mapWidth;  
            float normalizedZ = (player.position.z - mapPosition.z) / mapHeight;
            
            // Convert to minimap coordinates
            int playerMinimapX = minimapX + (int)(normalizedX * cubicmap.width * minimapScale);
            int playerMinimapY = minimapY + (int)(normalizedZ * cubicmap.height * minimapScale);
            
            // Draw player on minimap
            DrawRectangle(playerMinimapX, playerMinimapY, 4, 4, RED);
            
            // Draw enemies on minimap
            for (int i = 0; i < enemyCount; i++) {
                if (enemies[i].active) {
                    float enemyNormalizedX = (enemies[i].position.x - mapPosition.x) / mapWidth;
                    float enemyNormalizedZ = (enemies[i].position.z - mapPosition.z) / mapHeight;
                    
                    int enemyMinimapX = minimapX + (int)(enemyNormalizedX * cubicmap.width * minimapScale);
                    int enemyMinimapY = minimapY + (int)(enemyNormalizedZ * cubicmap.height * minimapScale);
                    
                    DrawRectangle(enemyMinimapX, enemyMinimapY, 3, 3, PURPLE);
                }
            }

            // Draw controls help
            DrawText("Controls: WASD to move, SPACE to jump, Mouse wheel to zoom", 10, screenHeight - 70, 20, WHITE);
            DrawText("Left-click to shoot, P to pause, F to toggle wireframe", 10, screenHeight - 50, 20, WHITE);
            
            // Display player health
            DrawText("HEALTH:", 10, 80, 20, WHITE);
            DrawRectangle(100, 80, player.health, 20, (Color){ 255, (unsigned char)(player.health * 2.55f), 0, 255 });
            DrawRectangleLines(100, 80, 100, 20, WHITE);
            
            // Display player position and physics for debugging
            DrawText(TextFormat("Position: (%.2f, %.2f, %.2f)", player.position.x, player.position.y, player.position.z), 10, 30, 20, YELLOW);
            DrawText(TextFormat("Velocity: (%.2f, %.2f, %.2f)", player.velocity.x, player.velocity.y, player.velocity.z), 10, 50, 20, YELLOW);
            DrawFPS(10, 10);

            // Draw game state
            if (gameState == GAME_PAUSED)
            {
                DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.6f));
                
                if (player.health <= 0) {
                    DrawText("GAME OVER", screenWidth/2 - MeasureText("GAME OVER", 40)/2, screenHeight/2 - 40, 40, RED);
                    DrawText("PRESS ESC TO QUIT", screenWidth/2 - MeasureText("PRESS ESC TO QUIT", 20)/2, screenHeight/2 + 10, 20, WHITE);
                } else {
                    DrawText("GAME PAUSED", screenWidth/2 - MeasureText("GAME PAUSED", 40)/2, screenHeight/2 - 40, 40, WHITE);
                    DrawText("PRESS P TO RESUME", screenWidth/2 - MeasureText("PRESS P TO RESUME", 20)/2, screenHeight/2 + 10, 20, WHITE);
                }
            }

        EndDrawing();
    }

    UnloadTexture(cubicmap);
    UnloadTexture(texture);
    UnloadModel(model);
    UnloadTexture(player.texture);
    UnloadTexture(enemyTexture);
    
    CloseWindow();
    return 0;
}

BoundingBox GetPlayerBoundingBox(Player player)
{
    return (BoundingBox){
        (Vector3){ player.position.x - player.size.x/2, player.position.y, player.position.z - player.size.z/2 },
        (Vector3){ player.position.x + player.size.x/2, player.position.y + player.size.y, player.position.z + player.size.z/2 }
    };
}

BoundingBox GetEnemyBoundingBox(Enemy enemy)
{
    return (BoundingBox){
        (Vector3){ enemy.position.x - enemy.size.x/2, enemy.position.y, enemy.position.z - enemy.size.z/2 },
        (Vector3){ enemy.position.x + enemy.size.x/2, enemy.position.y + enemy.size.y, enemy.position.z + enemy.size.z/2 }
    };
}

// Updated to handle only horizontal and ground collisions
bool CheckCollisionPlayerWithMap(Player *player, Model model, Vector3 mapPosition, Texture2D cubicmap)
{
    // First, check if player is within the overall map bounds
    BoundingBox mapBounds = GetModelBoundingBox(model);
    mapBounds.min = Vector3Add(mapBounds.min, mapPosition);
    mapBounds.max = Vector3Add(mapBounds.max, mapPosition);
    
    // Get player bounding box
    BoundingBox playerBounds = GetPlayerBoundingBox(*player);
    
    // Check if player is outside the map bounds
    if (!CheckCollisionBoxes(playerBounds, mapBounds))
        return true;
        
    // More precise collision detection with individual cubes
    // Calculate the grid cell position where the player is currently located
    int cellX = (int)(player->position.x - mapPosition.x);
    int cellZ = (int)(player->position.z - mapPosition.z);
    
    // We need the image data to check pixel colors
    Image cubicmapImage = LoadImageFromTexture(cubicmap);
    
    bool collision = false;
    bool groundContact = false;
    
    // Check the surrounding cells for collisions (3x3 grid around player)
    for (int z = -1; z <= 1; z++)
    {
        for (int x = -1; x <= 1; x++)
        {
            int currentCellX = cellX + x;
            int currentCellZ = cellZ + z;
            
            // Skip cells outside the map bounds
            if (currentCellX < 0 || currentCellZ < 0 || 
                currentCellX >= cubicmap.width || currentCellZ >= cubicmap.height)
                continue;
            
            // Check if the current cell is a wall (white pixel in the cubicmap)
            Color cellColor = GetImageColor(cubicmapImage, currentCellX, currentCellZ);
            
            // If the cell is a wall (white or very light color)
            if ((cellColor.r > 50) && (cellColor.g > 50) && (cellColor.b > 50))
            {
                // Create a box for the current wall cell
                float cellXOffset = currentCellX - 0.5f;
                float cellZOffset = currentCellZ - 0.5f;
                BoundingBox cellBounds = {
                    (Vector3){ mapPosition.x + cellXOffset, mapPosition.y, mapPosition.z + cellZOffset },
                    (Vector3){ mapPosition.x + cellXOffset + 1.0f, mapPosition.y + 1.0f, mapPosition.z + cellZOffset + 1.0f }
                };
                
                // Check collision between player and the current wall cell
                if (CheckCollisionBoxes(playerBounds, cellBounds))
                {
                    // Check if this is a ground collision
                    float playerBottomY = player->position.y;
                    float cellTopY = mapPosition.y + 1.0f;
                    
                    if (fabs(playerBottomY - cellTopY) < 0.1f)
                    {
                        groundContact = true;
                    }
                    else
                    {
                        collision = true;
                    }
                }
            }
        }
    }
    
    // Update player's grounded state based on ground contact
    if (groundContact || player->position.y <= 0.5f)  // Also check if player is at the base height
    {
        player->isGrounded = true;
    }
    
    UnloadImage(cubicmapImage);
    
    return collision;
}

// Similar function for enemies
bool CheckCollisionEnemyWithMap(Enemy *enemy, Model model, Vector3 mapPosition, Texture2D cubicmap)
{
    // Get enemy bounding box
    BoundingBox enemyBounds = GetEnemyBoundingBox(*enemy);
    
    // Get map bounds
    BoundingBox mapBounds = GetModelBoundingBox(model);
    mapBounds.min = Vector3Add(mapBounds.min, mapPosition);
    mapBounds.max = Vector3Add(mapBounds.max, mapPosition);
    
    // Check if enemy is outside the map bounds
    if (!CheckCollisionBoxes(enemyBounds, mapBounds))
        return true;
        
    // More precise collision detection with individual cubes
    int cellX = (int)(enemy->position.x - mapPosition.x);
    int cellZ = (int)(enemy->position.z - mapPosition.z);
    
    // We need the image data to check pixel colors
    Image cubicmapImage = LoadImageFromTexture(cubicmap);
    
    bool collision = false;
    
    // Check the surrounding cells for collisions (3x3 grid around enemy)
    for (int z = -1; z <= 1; z++)
    {
        for (int x = -1; x <= 1; x++)
        {
            int currentCellX = cellX + x;
            int currentCellZ = cellZ + z;
            
            // Skip cells outside the map bounds
            if (currentCellX < 0 || currentCellZ < 0 || 
                currentCellX >= cubicmap.width || currentCellZ >= cubicmap.height)
                continue;
            
            // Check if the current cell is a wall (white pixel in the cubicmap)
            Color cellColor = GetImageColor(cubicmapImage, currentCellX, currentCellZ);
            
            // If the cell is a wall (white or very light color)
            if ((cellColor.r > 50) && (cellColor.g > 50) && (cellColor.b > 50))
            {
                // Create a box for the current wall cell
                float cellXOffset = currentCellX - 0.5f;
                float cellZOffset = currentCellZ - 0.5f;
                BoundingBox cellBounds = {
                    (Vector3){ mapPosition.x + cellXOffset, mapPosition.y, mapPosition.z + cellZOffset },
                    (Vector3){ mapPosition.x + cellXOffset + 1.0f, mapPosition.y + 1.0f, mapPosition.z + cellZOffset + 1.0f }
                };
                
                // Check collision between enemy and the current wall cell
                if (CheckCollisionBoxes(enemyBounds, cellBounds))
                {
                    collision = true;
                    break;
                }
            }
        }
        if (collision) break;
    }
    
    UnloadImage(cubicmapImage);
    
    return collision;
}

// Check if bullet collides with map
bool CheckCollisionBulletWithMap(Bullet bullet, Model model, Vector3 mapPosition, Texture2D cubicmap)
{
    // Get map bounds
    BoundingBox mapBounds = GetModelBoundingBox(model);
    mapBounds.min = Vector3Add(mapBounds.min, mapPosition);
    mapBounds.max = Vector3Add(mapBounds.max, mapPosition);
    
    // Check if bullet is outside the map bounds
    if (!CheckCollisionSphereBox(
            bullet.position, 
            bullet.radius, 
            mapBounds))
        return true;
        
    // More precise collision detection with individual cubes
    int cellX = (int)(bullet.position.x - mapPosition.x);
    int cellZ = (int)(bullet.position.z - mapPosition.z);
    
    // We need the image data to check pixel colors
    Image cubicmapImage = LoadImageFromTexture(cubicmap);
    
    bool collision = false;
    
    // Check the surrounding cells for collisions (3x3 grid around bullet)
    for (int z = -1; z <= 1; z++)
    {
        for (int x = -1; x <= 1; x++)
        {
            int currentCellX = cellX + x;
            int currentCellZ = cellZ + z;
            
            // Skip cells outside the map bounds
            if (currentCellX < 0 || currentCellZ < 0 || 
                currentCellX >= cubicmap.width || currentCellZ >= cubicmap.height)
                continue;
            
            // Check if the current cell is a wall (white pixel in the cubicmap)
            Color cellColor = GetImageColor(cubicmapImage, currentCellX, currentCellZ);
            
            // If the cell is a wall (white or very light color)
            if ((cellColor.r > 50) && (cellColor.g > 50) && (cellColor.b > 50))
            {
                // Create a box for the current wall cell
                float cellXOffset = currentCellX - 0.5f;
                float cellZOffset = currentCellZ - 0.5f;
                BoundingBox cellBounds = {
                    (Vector3){ mapPosition.x + cellXOffset, mapPosition.y, mapPosition.z + cellZOffset },
                    (Vector3){ mapPosition.x + cellXOffset + 1.0f, mapPosition.y + 1.0f, mapPosition.z + cellZOffset + 1.0f }
                };
                
                // Check collision between bullet and the current wall cell
                if (CheckCollisionSphereBox(bullet.position, bullet.radius, cellBounds))
                {
                    collision = true;
                    break;
                }
            }
        }
        if (collision) break;
    }
    
    UnloadImage(cubicmapImage);
    
    return collision;
}

// Check collision between a sphere and a box
// Returns true if the sphere and box collide, false otherwise
bool CheckCollisionSphereBox(Vector3 center, float radius, BoundingBox box)
{
    // Find the closest point to the sphere within the box
    Vector3 closest = {
        fmaxf(box.min.x, fminf(center.x, box.max.x)),
        fmaxf(box.min.y, fminf(center.y, box.max.y)),
        fmaxf(box.min.z, fminf(center.z, box.max.z))
    };
    
    // Calculate the distance between the sphere's center and the closest point
    float distanceSquared = 
        powf(closest.x - center.x, 2.0f) + 
        powf(closest.y - center.y, 2.0f) + 
        powf(closest.z - center.z, 2.0f);
    
    // If the distance is less than the radius squared, they collide
    return distanceSquared < (radius * radius);
}

// New function to handle player physics (gravity, jumping, vertical movement)
void UpdatePlayerPhysics(Player *player, float deltaTime, Model model, Vector3 mapPosition, Texture2D cubicmap)
{
    // Apply gravity
    if (!player->isGrounded)
    {
        player->velocity.y -= player->gravity;
    }
    else if (player->velocity.y < 0)
    {
        // Reset vertical velocity when grounded
        player->velocity.y = 0;
    }
    
    // Apply vertical velocity to position
    player->position.y += player->velocity.y;
    
    // Check if player is below ground level
    if (player->position.y < 0.5f)  // Assuming 0.5f is ground level
    {
        player->position.y = 0.5f;
        player->velocity.y = 0;
        player->isGrounded = true;
    }
    else
    {
        // Check for collisions with the map
        player->isGrounded = false;  // Assume not grounded until proven otherwise
        
        // Only check for ground collisions, not ceiling collisions
        if (player->velocity.y < 0)  // Only when falling
        {
            if (CheckCollisionPlayerWithMap(player, model, mapPosition, cubicmap))
            {
                // We hit something while falling, but it wasn't the ground
                // This is for horizontal collisions during fall
            }
            
            // Check if we've landed on ground
            // Get player bounding box
            BoundingBox playerBounds = GetPlayerBoundingBox(*player);
            
            // We need the image data to check pixel colors
            Image cubicmapImage = LoadImageFromTexture(cubicmap);
            
            // Calculate the grid cell position where the player is currently located
            int cellX = (int)(player->position.x - mapPosition.x);
            int cellZ = (int)(player->position.z - mapPosition.z);
            
            // Check for ground beneath player
            for (int z = -1; z <= 1; z++)
            {
                for (int x = -1; x <= 1; x++)
                {
                    int currentCellX = cellX + x;
                    int currentCellZ = cellZ + z;
                    
                    // Skip cells outside the map bounds
                    if (currentCellX < 0 || currentCellZ < 0 || 
                        currentCellX >= cubicmap.width || currentCellZ >= cubicmap.height)
                        continue;
                    
                    // Check if the current cell is a wall (white pixel in the cubicmap)
                    Color cellColor = GetImageColor(cubicmapImage, currentCellX, currentCellZ);
                    
                    // If the cell is a wall (white or very light color)
                    if ((cellColor.r > 50) && (cellColor.g > 50) && (cellColor.b > 50))
                    {
                        // Create a box for the current wall cell
                        float cellXOffset = currentCellX - 0.5f;
                        float cellZOffset = currentCellZ - 0.5f;
                        BoundingBox cellBounds = {
                            (Vector3){ mapPosition.x + cellXOffset, mapPosition.y, mapPosition.z + cellZOffset },
                            (Vector3){ mapPosition.x + cellXOffset + 1.0f, mapPosition.y + 1.0f, mapPosition.z + cellZOffset + 1.0f }
                        };
                        
                        // Check if player's bottom is at or slightly above the cell's top
                        if (playerBounds.min.y <= cellBounds.max.y && 
                            playerBounds.min.y >= cellBounds.max.y - 0.1f &&
                            CheckCollisionBoxes(playerBounds, cellBounds))
                        {
                            player->isGrounded = true;
                            player->position.y = cellBounds.max.y;
                            player->velocity.y = 0;
                            break;
                        }
                    }
                }
                if (player->isGrounded) break;
            }
            
            UnloadImage(cubicmapImage);
        }
    }
}

// Update camera to follow player
void UpdateGameCamera(Camera *camera, Player player)
{
    // Update camera position to follow player from a top-down perspective
    camera->position.x = player.position.x;
    camera->position.z = player.position.z + 10.0f;
    camera->target = player.position;
}

// Shoot bullet from a position in a direction
void ShootBullet(Bullet *bullets, int *bulletCount, Vector3 position, Vector3 direction, bool fromPlayer)
{
    // Don't shoot if bullet array is full
    if (*bulletCount >= MAX_BULLETS) return;
    
    // Find an inactive bullet slot
    int bulletIndex = -1;
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (!bullets[i].active)
        {
            bulletIndex = i;
            break;
        }
    }
    
    // If no inactive bullet found, use the oldest one
    if (bulletIndex == -1)
    {
        bulletIndex = *bulletCount % MAX_BULLETS;
    }
    else
    {
        (*bulletCount)++;
    }
    
    // Initialize the bullet
    bullets[bulletIndex].position = position;
    bullets[bulletIndex].direction = direction;
    bullets[bulletIndex].speed = BULLET_SPEED;
    bullets[bulletIndex].radius = BULLET_RADIUS;
    bullets[bulletIndex].active = true;
    bullets[bulletIndex].fromPlayer = fromPlayer;
    
    // Set color based on who shot it
    bullets[bulletIndex].color = fromPlayer ? YELLOW : RED;
}

// Update all active bullets
void UpdateBullets(Bullet *bullets, int bulletCount, float deltaTime)
{
    for (int i = 0; i < bulletCount; i++)
    {
        if (bullets[i].active)
        {
            // Move bullet in its direction
            bullets[i].position.x += bullets[i].direction.x * bullets[i].speed;
            bullets[i].position.y += bullets[i].direction.y * bullets[i].speed;
            bullets[i].position.z += bullets[i].direction.z * bullets[i].speed;
            
            // Deactivate bullets that go too far (prevent infinitely traveling bullets)
            float distance = Vector3Length(bullets[i].position);
            if (distance > 50.0f)
            {
                bullets[i].active = false;
            }
        }
    }
}

// Update enemies behavior
void UpdateEnemies(Enemy *enemies, int enemyCount, Player player, Bullet *bullets, int *bulletCount, float deltaTime, Model model, Vector3 mapPosition, Texture2D cubicmap)
{
    for (int i = 0; i < enemyCount; i++)
    {
        if (enemies[i].active)
        {
            // Update hit timer for enemy
            if (enemies[i].isHit)
            {
                enemies[i].hitTimer -= deltaTime;
                if (enemies[i].hitTimer <= 0.0f)
                {
                    enemies[i].isHit = false;
                }
            }
            
            // Simple AI - move towards player if not too close
            Vector3 direction = Vector3Subtract(player.position, enemies[i].position);
            float distance = Vector3Length(direction);
            
            // Normalize direction
            if (distance > 0)
            {
                direction = Vector3Scale(Vector3Normalize(direction), enemies[i].speed);
            }
            
            // Only move if not too close to player
            if (distance > 3.0f)
            {
                // Store previous position
                Vector3 previousPosition = enemies[i].position;
                
                // Move towards player
                enemies[i].position.x += direction.x;
                
                // Check for collision after x movement
                if (CheckCollisionEnemyWithMap(&enemies[i], model, mapPosition, cubicmap))
                {
                    // Restore x position if collision detected
                    enemies[i].position.x = previousPosition.x;
                }
                
                // Move in z direction separately
                enemies[i].position.z += direction.z;
                
                // Check for collision after z movement
                if (CheckCollisionEnemyWithMap(&enemies[i], model, mapPosition, cubicmap))
                {
                    // Restore z position if collision detected
                    enemies[i].position.z = previousPosition.z;
                }
            }
            
            // Shooting logic - enemies shoot at player periodically
            enemies[i].shootTimer -= deltaTime;
            if (enemies[i].shootTimer <= 0 && distance < 10.0f) // Only shoot if player is within range
            {
                // Calculate direction to player
                Vector3 shootDirection = Vector3Normalize(Vector3Subtract(
                    (Vector3){ player.position.x, player.position.y + 0.5f, player.position.z },
                    (Vector3){ enemies[i].position.x, enemies[i].position.y + 0.5f, enemies[i].position.z }
                ));
                
                // Shoot bullet at player
                ShootBullet(bullets, bulletCount, 
                    (Vector3){ enemies[i].position.x, enemies[i].position.y + 0.5f, enemies[i].position.z },
                    shootDirection, false);
                
                // Reset shoot timer with random variation
                enemies[i].shootTimer = enemies[i].shootCooldown + GetRandomValue(-50, 50) / 100.0f;
            }
        }
    }
}

// Check all bullet collisions with player, enemies, and map
void CheckBulletCollisions(Bullet *bullets, int bulletCount, Player *player, Enemy *enemies, int enemyCount, Model model, Vector3 mapPosition, Texture2D cubicmap)
{
    for (int i = 0; i < bulletCount; i++)
    {
        if (bullets[i].active)
        {
            // Check collision with map
            if (CheckCollisionBulletWithMap(bullets[i], model, mapPosition, cubicmap))
            {
                bullets[i].active = false;
                continue;
            }
            
            // Check collision with player (only enemy bullets)
            if (!bullets[i].fromPlayer)
            {
                BoundingBox playerBox = GetPlayerBoundingBox(*player);
                if (CheckCollisionSphereBox(bullets[i].position, bullets[i].radius, playerBox))
                {
                    // Player hit by enemy bullet
                    if (!player->isHit) // Only take damage if not in hit state
                    {
                        player->health -= 10;
                        player->isHit = true;
                        player->hitTimer = 0.5f; // Invulnerability period
                    }
                    
                    bullets[i].active = false;
                    continue;
                }
            }
            
            // Check collision with enemies (only player bullets)
            if (bullets[i].fromPlayer)
            {
                for (int j = 0; j < enemyCount; j++)
                {
                    if (enemies[j].active)
                    {
                        BoundingBox enemyBox = GetEnemyBoundingBox(enemies[j]);
                        if (CheckCollisionSphereBox(bullets[i].position, bullets[i].radius, enemyBox))
                        {
                            // Enemy hit by player bullet
                            if (!enemies[j].isHit) // Only take damage if not in hit state
                            {
                                enemies[j].health -= 10;
                                enemies[j].isHit = true;
                                enemies[j].hitTimer = 0.2f; // Shorter invulnerability than player
                                
                                // Check if enemy is defeated
                                if (enemies[j].health <= 0)
                                {
                                    enemies[j].active = false;
                                }
                            }
                            
                            bullets[i].active = false;
                            break;
                        }
                    }
                }
            }
        }
    }
}