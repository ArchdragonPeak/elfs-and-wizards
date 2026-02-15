// raylib
#include "raylib.h"
#include "raymath.h"
#include "rcamera.h"

// std
#include <array>
#include <chrono>
#include <iostream>
#include <math.h>
#include <memory>
#include <queue>
#include <sstream>
#include <stdlib.h>
#include <vector>
#include <thread>

// mine
#include "colors.hpp"

using namespace std;

// window related constants
constexpr int windowWidth = 1920;
constexpr int windowHeight = 1080;

// chunks
constexpr int BLOCK_PIXEL = 16;                         // Pixel per Block
constexpr int CHUNK_BLOCKS = 64;                        // Blocks per Chunk
constexpr int CHUNK_PIXEL = BLOCK_PIXEL * CHUNK_BLOCKS; // total Pixel per Block
int VIEW_RADIUS = 8;
Texture2D solidsT[609] = {}; //todo: allocate dynamically

bool operator==(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

struct Tuple
{
  int x;
  int y;
};

struct WorldInteraction
{
  int InteractionType;
  Tuple InteractionChunk;
  Tuple InteractionPosition;
  int BlockID;
};

struct Player
{
  static queue<WorldInteraction> interactions;
  string name;
  uint32_t id;
  Vector2 position;
  Tuple lastPlayerChunk;
  Tuple currentPlayerChunk;
  Vector2 velocity;
  Vector2 acceleration;
  bool onGround;
  float jumpHeight = -1400.0f;
  Texture2D playerTexture;
  int currentItem;
};
queue<WorldInteraction> Player::interactions;

struct Chunk
{
  static Tuple selectedChunkPixel;
  size_t id; // should be usable to derive chunkPos
  Tuple chunkPos; // chunk coordinates
  bool needsUpdate = true; // as data on chunks need frecuent updates this can possibly also be used as isGenerated
  bool isLoaded = false;
  Image chunkData; // blocks as pixels in Image (RAM)
  Texture2D chunkDataTexture; // stores image from above to VRAM, only for debug
  RenderTexture2D chunkTexture; // baked texture
};
Tuple Chunk::selectedChunkPixel = {0, 0};

struct World
{
  string name;
  const int version = 0;
  const int width = 64;
  const int height = 64;
  bool isLoaded = false;
  bool isGenerated = false;
  uint8_t type = 1; // ex: 0 - overworld, 1 - mines: more to be added
  array<array<unique_ptr<Chunk>, 64>, 64> chunkArr;
  vector<Player> playerList;
};
const int surfaceFactor = 20;


void generateChunkDataPerlin(Chunk &chunk, int xOffset, int yOffset)
{
  // 1: generate perlin image
  chunk.chunkData = GenImagePerlinNoise(CHUNK_BLOCKS, CHUNK_BLOCKS, xOffset, yOffset, 2);
  
  // 2: check if generated blocks are over surface function (sinus), if so, set color to black
  for(size_t y = 0; y<chunk.chunkData.height;y++)
  {
    for(size_t x = 0; x<chunk.chunkData.width;x++)
    {
      // getting pointer on raw image data
      int realX = x + xOffset;
      int realY = y + yOffset;
      Color *ptr = (Color *)chunk.chunkData.data;
      if((1000+(sin(realX*0.01f)*surfaceFactor)) < realY)
      {
        //ptr[y * chunk.chunkData.width + x] = (Color){255, 255, 255, 255};
        // take the bits of the pixel that are responsible for texture-id and
        // perform step fn.
        Color *dings = &ptr[y*chunk.chunkData.width + x];
        ptr[y*chunk.chunkData.width + x] =
          (dings->r < 100) ? (Color){1,160,255,255} : (Color){1,176,255,255};
        
        continue;
      }

      ptr[y*chunk.chunkData.width + x] = (Color){0, 0, 0, 255};
    }
  }
}


void generateChunkData(Chunk &chunk)
{
  // paint chunk with random colors
  for (size_t y = 0; y < chunk.chunkData.height; y++)
  {
    for (size_t x = 0; x < chunk.chunkData.width; x++)
    {
      Color *ptr = (Color *)chunk.chunkData.data;
      uint16_t rC = (uint16_t)rand() % 610; // rand() % [Number of Textures+1]
      Color randC = {(unsigned char)(rC >> 4), (unsigned char)((rC & 0xF) << 4), 0, 0};
      ptr[y * chunk.chunkData.width + x] = randC;
      //ptr[y * chunk.chunkData.width + x] = getRandomColor();
    }
  }
}

// creating a chunk struct on the pointer, allocating data and RenderTexture for future painting
void initChunk(Chunk &chunk, size_t id, Tuple chunkPos)
{
  chunk.id = id;
  chunk.chunkPos = chunkPos;
  chunk.chunkData.width = CHUNK_BLOCKS;
  chunk.chunkData.height = CHUNK_BLOCKS;
  chunk.chunkData.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
  chunk.chunkData.mipmaps = 1;
  // 64*64*4 = 16384 b
  chunk.chunkData.data = calloc(chunk.chunkData.width * chunk.chunkData.height, sizeof(Color));
  chunk.isLoaded = true;
}

void handleInput(Player &p, Camera2D &camera)
{
  p.acceleration = {0.0f, 0.0f};
  const float accel = 2000.0f;

  if (IsKeyDown(KEY_D))
    p.acceleration.x += accel;
  if (IsKeyDown(KEY_A))
    p.acceleration.x -= accel;

  if (IsKeyDown(KEY_S))
    p.acceleration.y += accel;
  if (IsKeyDown(KEY_W))
    p.acceleration.y -= accel;

  if (IsKeyPressed(KEY_Q) && p.currentItem > 0)
    p.currentItem--;
  if (IsKeyPressed(KEY_E) && p.currentItem < 608)
    p.currentItem++;
  
  if (IsKeyPressed(KEY_UP) && VIEW_RADIUS < 65)
      VIEW_RADIUS += 1;
  
  if (IsKeyPressed(KEY_DOWN) && VIEW_RADIUS > 0)
      VIEW_RADIUS -= 1;
  
  
  // if (IsKeyDown(KEY_UP) && p.onGround)
  //   p.velocity.y = p.jumpHeight;

  if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
  {
    WorldInteraction interaction = {};
    interaction.BlockID = p.currentItem;
    interaction.InteractionChunk = p.currentPlayerChunk;
    interaction.InteractionPosition = Chunk::selectedChunkPixel;
    interaction.InteractionType = 0;
    Player::interactions.push(interaction);
  }
  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
  {
    const Vector2 mouseWorld = GetScreenToWorld2D(GetMousePosition(), camera);
    p.position = mouseWorld;
    p.velocity = {0.0f, 0.0f};
  }
  float wheel = GetMouseWheelMove();
  if (wheel != 0.0f)
  {
    camera.zoom *= 1.0f + wheel * 0.01f;
  }
}

void updatePhysics(Player &p, float dt, array<array<unique_ptr<Chunk>, 64>, 64> &chunks)
{
  // gravity
  // const float gravity = 3500.0f;
  // p.acceleration.y += gravity;

  // get velocity
  p.velocity = Vector2Add(p.velocity, Vector2Scale(p.acceleration, dt));

  // friction
  p.velocity.x *= 0.95f;

  // get position
  p.position = Vector2Add(p.position, Vector2Scale(p.velocity, dt));
  // cout << "Player: " << p.position.x << ", " << p.position.y << " | "
  //      << p.velocity.x << ", " << p.velocity.y << endl;
  //  test floor height

  /*
  // calculate wether player is on floor
  const float floorY = 2000.0f;
  if (p.position.y >= floorY)
  {
    p.position.y = floorY;
    p.velocity.y = 0.0f;
    p.onGround = true;
  }
  else
  {
    p.onGround = false;
  }*/

  // cout << "\rPlayer: " << p.position.x << ", " << p.position.y << " | "
  //      << p.velocity.x << ", " << p.velocity.y << flush;


  //  update player chunk by going through interactions
  int x = floor(p.position.x / CHUNK_PIXEL);
  int y = floor(p.position.y / CHUNK_PIXEL);
  if (!(x < 0 || x >= 64 || y < 0 || y >= 64))
    p.currentPlayerChunk = {x, y};
  while (!Player::interactions.empty())
  {
    WorldInteraction &interaction = Player::interactions.front();
    if (interaction.InteractionType == 0)
    {
      uint16_t rC = interaction.BlockID;
      //cout << "Interaction: " << interaction.InteractionChunk.x << ", " << interaction.InteractionChunk.y << endl;
      ImageDrawPixel(
        &chunks[interaction.InteractionChunk.y][interaction.InteractionChunk.x]->chunkData,
        interaction.InteractionPosition.x,
        interaction.InteractionPosition.y,
        (Color) {(unsigned char)(rC >> 4),(unsigned char)((rC & 0xF) << 4), 0, 0}
      );
      chunks[interaction.InteractionChunk.y][interaction.InteractionChunk.x]->needsUpdate = true;
    }
    Player::interactions.pop();
  }
}
void bakeChunk(Chunk &chunk)
{
  BeginTextureMode(chunk.chunkTexture);
  for (int y = 0; y < CHUNK_BLOCKS; y++)
  {
    for (int x = 0; x < CHUNK_BLOCKS; x++)
    {
      Color pixelColor = GetImageColor(chunk.chunkData, x, y);
      uint16_t id = (pixelColor.r << 4) | (pixelColor.g >> 4);
      if(id >= 609) id = 1;
      DrawTextureRec(
        solidsT[id],
        (Rectangle){0, 0, BLOCK_PIXEL, BLOCK_PIXEL},
        (Vector2){(float)x * BLOCK_PIXEL, (float)y * BLOCK_PIXEL},
        WHITE
      );
    }
  }
  EndTextureMode();
}
void draw(Player &p, array<array<unique_ptr<Chunk>, 64>, 64> &chunks, Camera2D &camera)
{
  BeginDrawing();
  ClearBackground(BlueSpace);
  BeginMode2D(camera);

  // draw data of chunks - goes through all chunks in chunks array in draws blocks as pixels
  const int dings = VIEW_RADIUS;
  
  for (size_t y = 0; y < chunks.size(); y++)
  {
    for (size_t x = 0; x < chunks[y].size(); x++)
    {
      if (!chunks[y][x])
        continue;

      Chunk &chunk = *chunks[y][x];
      const float scale = float(CHUNK_PIXEL) / 64.0f;
      DrawTextureEx(chunk.chunkDataTexture,
                   {(float)chunk.chunkPos.x * CHUNK_PIXEL, (float)chunk.chunkPos.y * CHUNK_PIXEL}, 0.0f, 16.0f, WHITE);
    }
  }
  /*
   * draw baked textures on chunks
   */
  const int half_chunk_size = VIEW_RADIUS;

  for (int y = -half_chunk_size; y <= half_chunk_size; y++)
  {
    // deactivate drawing of textures
    //continue;
    for (int x = -half_chunk_size; x <= half_chunk_size; x++)
    {
      const int chunkX = p.currentPlayerChunk.x + x;
      const int chunkY = p.currentPlayerChunk.y + y;

      if (chunkX < 0 || chunkX >= 64 || chunkY < 0 || chunkY >= 64)
        continue;
      if (chunks[chunkY][chunkX])
      {
        Chunk &chunk = *chunks[chunkY][chunkX];
        
        if(chunk.chunkTexture.texture.id == 0)
        {
          //cout << "trying to draw chunk with no texture" << "\n";
          continue;
        }
        
        const Tuple chunkPos = {chunkX * CHUNK_PIXEL, chunkY * CHUNK_PIXEL};
        // flipping the texture vertically
        DrawTextureRec(
          chunk.chunkTexture.texture,
          (Rectangle){0, 0, (float)chunk.chunkTexture.texture.width, -(float)chunk.chunkTexture.texture.height},
          {(float)chunkPos.x, (float)chunkPos.y},
          WHITE
        );
        //cout << "Chunk at (" << chunkX << ", " << chunkY << ") drawn." << endl;
      }
      else
      {
        cout << "This should not happen :/";
      }
    }
  }

  // visualize chunk player is in
  DrawRectangleLinesEx(
    {(float)p.currentPlayerChunk.x * CHUNK_PIXEL, (float)p.currentPlayerChunk.y * CHUNK_PIXEL,
    CHUNK_PIXEL, CHUNK_PIXEL},
    5,
    {255, 255, 255, 96}
  );

  // visualize selected block in playerchunk
  Vector2 mousePos = GetScreenToWorld2D(GetMousePosition(), camera);
  mousePos.x = mousePos.x - ((int)mousePos.x % BLOCK_PIXEL);
  mousePos.y = mousePos.y - ((int)mousePos.y % BLOCK_PIXEL);
  DrawRectangle(mousePos.x, mousePos.y, BLOCK_PIXEL, BLOCK_PIXEL, {255, 255, 255, 180});
  Tuple chunkWorldPos = {p.currentPlayerChunk.x * CHUNK_PIXEL, p.currentPlayerChunk.y * CHUNK_PIXEL};
  DrawRectangle(chunkWorldPos.x, chunkWorldPos.y, BLOCK_PIXEL, BLOCK_PIXEL, {255, 0, 255, 255});
  Chunk::selectedChunkPixel = {(int)(mousePos.x - chunkWorldPos.x) / 16, (int)(mousePos.y - chunkWorldPos.y) / 16};
  /*
  if(Chunk::selectedChunkPixel.x < 0) Chunk::selectedChunkPixel.x = 0;
  if(Chunk::selectedChunkPixel.y < 0) Chunk::selectedChunkPixel.y = 0;
  if(Chunk::selectedChunkPixel.x >= CHUNK_BLOCKS) Chunk::selectedChunkPixel.x = CHUNK_BLOCKS - 1;
  if(Chunk::selectedChunkPixel.y >= CHUNK_BLOCKS) Chunk::selectedChunkPixel.y = CHUNK_BLOCKS - 1;
  */
  // cout << Chunk::selectedChunkPixel.x << ", " << Chunk::selectedChunkPixel.y << endl;
  //  player
  DrawRectangle(p.position.x - 20, p.position.y - 40, 40, 80, OrangePeel);
  DrawTextureEx(p.playerTexture, {p.position.x - 25, p.position.y - 60}, 0.0f, 0.8f, WHITE);

  // tool
  // DrawRectanglePro({p.position.x, p.position.y}, {}, 0.0f, GreenReseda);

  // Map border lines
  DrawLine(0, 0, CHUNK_PIXEL * 64, 0, WHITE);
  DrawLine(0, 0, 0, CHUNK_PIXEL * 64, WHITE);
  DrawLine(0, CHUNK_PIXEL * 64, CHUNK_PIXEL * 64, CHUNK_PIXEL * 64, WHITE);
  DrawLine(CHUNK_PIXEL * 64, 0, CHUNK_PIXEL * 64, CHUNK_PIXEL * 64, WHITE);

  EndMode2D();

  // GUI

  // items and stuff
  float halfWindowWidth = windowWidth / 2;
  Rectangle texSource = {0, 0, 16, 16};
  float distToTop = 100;

  // background
  DrawRectanglePro({halfWindowWidth, distToTop, 300, 120}, {150, 60}, 0.0f, {255, 255, 255, 200});

  // center item
  float centerItemDimension = 100;
  Rectangle centerItemDest = {halfWindowWidth, distToTop, centerItemDimension, centerItemDimension};
  Vector2 centerOrigin = {centerItemDimension / 2, centerItemDimension / 2};
  DrawTexturePro(solidsT[p.currentItem], texSource, centerItemDest, {50, 50}, 0, WHITE);

  // outer items
  float outerItemDimension = 60;
  Rectangle outerItemDestL = {halfWindowWidth - centerItemDimension, distToTop, outerItemDimension, outerItemDimension};
  Rectangle outerItemDestR = {halfWindowWidth + centerItemDimension, distToTop, outerItemDimension, outerItemDimension};
  Vector2 outerOrigin = {outerItemDimension / 2, outerItemDimension / 2};
  int prevItem = p.currentItem - 1;
  int nextItem = p.currentItem + 1;
  if (prevItem >= 0)
    DrawTexturePro(solidsT[prevItem], texSource, outerItemDestL, outerOrigin, 0, WHITE);
  if (nextItem <= 609)
    DrawTexturePro(solidsT[nextItem], texSource, outerItemDestR, outerOrigin, 0, WHITE);

  DrawRectangle(5, 5, 400, 200, {255, 255, 255, 200});
  DrawFPS(10, 10);
  string zoomStr = "Zoom " + to_string(camera.zoom);
  DrawText(zoomStr.c_str(), 10, 30, 20, GreenPoly);

  string chunkStr = "Chunk-Width: " + to_string(VIEW_RADIUS);
  DrawText(chunkStr.c_str(), 10, 50, 20, GreenPoly);

  ostringstream playerStream;
  playerStream << "Player: " << p.position.x << ", " << p.position.y << " | " << p.velocity.x << ", " << p.velocity.y;
  DrawText(playerStream.str().c_str(), 10, 70, 20, GreenPoly);
  DrawText(TextFormat("Selected item: %d", p.currentItem), 10, 90, 20, GreenPoly);

  EndDrawing();
}

/*
 * The idea is a very basic model: keep all chunk images in memory,
 * while uploading a small amount to VRAM.
 * At 64x64 Chunks per World that shouldnt be more than a few hundred mb.
 * todo: needs integration in the world-->chunks Model
 */
bool canBakeAnotherChunk(size_t& chunksGenerated, double startTime, double budget)
{
  if(chunksGenerated == 0) return true;
  const double elapsed = (GetTime() - startTime) * 1000.0;
  return elapsed < budget;
}

void manageChunks(Player &p, array<array<unique_ptr<Chunk>, 64>, 64> &chunks)
{
  // frame budget stuff
  const double budget = 0.05; // milliseconds
  const double startTime = GetTime();
  size_t chunksGenerated = 0;
  
  // load moore neighbours (more [lmao] or less) of chunk the player is in
  Tuple playerChunk = p.currentPlayerChunk;
  // if (p.currentPlayerChunk.x == p.lastPlayerChunk.x && p.currentPlayerChunk.y == p.lastPlayerChunk.y)
  //   return;

  // todo: this maybe should be in physics
  p.lastPlayerChunk.x = p.currentPlayerChunk.x;
  p.lastPlayerChunk.y = p.currentPlayerChunk.y;

  // chunks are loaded in a box-like structure with the witdth of VIEW_RADIUS + 1
  const int half_width = VIEW_RADIUS;
  for (int y = -half_width; y <= half_width; y++)
  {
    for (int x = -half_width; x <= half_width; x++)
    {
      const int chunkX = playerChunk.x + x;
      const int chunkY = playerChunk.y + y;

      if (chunkX < 0 || chunkX >= 64 || chunkY < 0 || chunkY >= 64)
        continue;
      // unique_ptr possibly unnecessary
      unique_ptr<Chunk> &chunkPtr = chunks[chunkY][chunkX];
      Chunk &chunk = *chunks[chunkY][chunkX];

      if (chunk.chunkTexture.id == 0)
      {
        if (canBakeAnotherChunk(chunksGenerated, startTime, budget))
        {
          chunk.chunkTexture = LoadRenderTexture(CHUNK_PIXEL, CHUNK_PIXEL);
          chunk.needsUpdate = true;
          chunksGenerated++;
        }
        else
        {
          continue;
        }
      }

      if (chunk.needsUpdate)
      {
        if (canBakeAnotherChunk(chunksGenerated, startTime, budget))
        {
          bakeChunk(chunk);
          chunk.needsUpdate = false;
          chunksGenerated++;
        }
      }
    }
  }

  // unload distant chunks
  const float dist = (CHUNK_PIXEL * (VIEW_RADIUS+5))*1.5;
  for (int y = 0; y < chunks.size(); y++)
  {
    for (int x = 0; x < chunks[y].size(); x++)
    {
      if (chunks[y][x])
      {
        Vector2 chunkPos = {(float)x * CHUNK_PIXEL + CHUNK_PIXEL / 2, (float)y * CHUNK_PIXEL + CHUNK_PIXEL / 2};
        if (Vector2Distance(chunkPos, p.position) > dist)
        {
          if(chunks[y][x]->chunkTexture.id != 0)
            UnloadRenderTexture(chunks[y][x]->chunkTexture);
          chunks[y][x]->chunkTexture = {};
        }
      }
    }
  }
}

int gameLoop(Player &p, vector<World> &worldList, Camera2D &camera)
{
  const float dt = 1.0f / 60.0f;
  float accumulator = 0.0f;
  while (!WindowShouldClose())
  {
    float frameTime = GetFrameTime();
    accumulator += frameTime;

    handleInput(p, camera);

    while (accumulator >= dt)
    {
      for(World& w : worldList){
        if(w.isLoaded)
          updatePhysics(p, dt, w.chunkArr);
      }
      accumulator -= dt;
    }

    for(World& w : worldList){
      if(w.isLoaded)
        manageChunks(p, w.chunkArr);
    }
    camera.target = {p.position.x, p.position.y};
    
    for(World& w : worldList){
      if(w.isLoaded)
        draw(p, w.chunkArr, camera);
    }
  }
  return 0;
}

// generats chunk-data for a World. Dont generate Textures yet.
// VRAM-lifecycle is completly managed by manageChunks (hopefully)
void generateWorld(World& world)
{
  const int worldWidth = world.width;
  const int worldHeight = world.height;
  
  unsigned int tcount = std::thread::hardware_concurrency();
  if (tcount == 0)
    tcount = 4;
  
  std::vector<std::thread> threads;
  threads.reserve(tcount);
  
  for (unsigned int t = 0; t < tcount; ++t)
  {
    
      threads.emplace_back([&, t]()
      {
        
          for (int y = (int)t; y < worldHeight; y += (int)tcount)
          {
            
              for (int x = 0; x < worldWidth; ++x)
              {
                  int i = y * worldWidth + x;
  
                  world.chunkArr[y][x] = std::make_unique<Chunk>();
                  Chunk& chunk = *world.chunkArr[y][x];
  
                  initChunk(chunk, i, {x, y});
                  generateChunkDataPerlin(chunk, x * CHUNK_BLOCKS, y * CHUNK_BLOCKS);
                  chunk.needsUpdate = true;
              }
          }
      });
  }
  
  for (auto& th : threads)
    th.join();
  
  for(int y = 0;y<worldHeight;y++)
  {
    
    for(int x = 0; x < worldWidth; x++)
    {
      Chunk &chunk = *world.chunkArr[y][x];
      chunk.chunkDataTexture = LoadTextureFromImage(chunk.chunkData); 
    }
  }
  
}

void generateWorldOld(World& world)
{
  const int worldWidth = world.width;
  const int worldHeight = world.height;
  
  for(int y = 0;y<worldHeight;y++)
  {
    
    for(int x = 0; x < worldWidth; x++)
    {
      world.chunkArr[y][x] = make_unique<Chunk>();
      Chunk &chunk = *world.chunkArr[y][x];

      initChunk(chunk, y * worldWidth + x, {x, y});

      // one block is one pixel in the data texture, thus we need to scale to block-level-resolution
      // which means 1 block = 1 unit
      generateChunkDataPerlin(chunk, x*CHUNK_BLOCKS, y*CHUNK_BLOCKS);
      //generateChunkData(chunk);
      chunk.chunkDataTexture = LoadTextureFromImage(chunk.chunkData);
      chunk.needsUpdate = true;
    }
  }
}

int main()
{
  // initializing
  InitWindow(windowWidth, windowHeight, "elfs-and-wizards");
  SetTargetFPS(360*2);

  vector<World> worldList;
  worldList.emplace_back();

  // setup player
  Player player = {
      .name = "Louis",
      .id = 1,
      //.position = {.x = 32 * 64 * 16, .y = 32 * 64 * 16},
      .position = {.x = 32 * 64, .y = 32 * 64}, 
      .playerTexture = LoadTexture("../assets/elf_character.png"),
      .currentItem = 607,
  };
  
  // setup world
  World& world = worldList.back();
  world.name = "Overworld";
  world.playerList.push_back(player);
  world.isLoaded = true;
  
  std::cout << "Initializing world..." << "\n";
  std::chrono::time_point<std::chrono::high_resolution_clock> worldGenStart = std::chrono::high_resolution_clock::now();
  for(World& world : worldList)
  {
    generateWorld(world);
  }
  std::chrono::time_point<std::chrono::high_resolution_clock> worldGenEnd = std::chrono::high_resolution_clock::now();
  long long worldGenMs = std::chrono::duration_cast<std::chrono::milliseconds>(worldGenEnd - worldGenStart).count();
  
  std::cout << "World generated" << std::endl;
  std::cout << "generating World took " << worldGenMs << " ms" << "\n";
  
  for (int i = 0; i < 609; i++)
  {
    string str = "../assets/solids/texture_16px ";
    str += to_string(i + 1);
    str += ".png";
    solidsT[i] = LoadTexture(str.c_str());
  }

  Camera2D camera = {0};
  camera.target = {player.position.x, player.position.y};
  camera.offset = {(1920 / 2) + 10, (1080 / 2) + 10};
  camera.rotation = 0.0f;
  camera.zoom = 1.0f;

  // start
  gameLoop(player, worldList, camera);

  CloseWindow();
  return 0;
}
