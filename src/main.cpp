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
#include <sstream>
#include <stdlib.h>
#include <vector>

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
int VIEW_RADIUS = 3;                                    // only even numbers please!
Texture2D solidsT[609] = {};

bool operator==(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

struct Tuple
{
  int x;
  int y;
};

struct Player
{
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

struct Chunk
{
  size_t id;
  Tuple chunkPos;
  bool needsUpdate = true;
  Image chunkData; // stores blocks
  Texture2D chunkDataTexture;
  RenderTexture2D chunkTexture;
};

Color palette[] = {{90, 110, 140, 255}, {120, 90, 70, 255}, {100, 130, 90, 255}};

Color getRandomColor()
{
  int size = sizeof(palette) / sizeof(palette[0]);
  return palette[rand() % size];
}

int paintChunk(Chunk &chunk)
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
      // ptr[y * chunk.chunkData.width + x] = getRandomColor();
    }
  }
  chunk.chunkDataTexture = LoadTextureFromImage(chunk.chunkData);
  // UnloadImage(chunk.data);
  return 0;
}
/*
Chunk initChunk(size_t id, Tuple chunkPos)
{
  Chunk chunk;
  chunk.id = id;
  chunk.chunkPos = chunkPos;
  chunk.data.width = CHUNK_BLOCKS;
  chunk.data.height = CHUNK_BLOCKS;
  chunk.data.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
  chunk.data.mipmaps = 8;
  // 64*64*4 = 16384
  chunk.data.data = calloc(chunk.data.width * chunk.data.height, sizeof(Color));
  return chunk;
}
*/

void initChunk(Chunk &chunk, size_t id, Tuple chunkPos)
{
  chunk.id = id;
  chunk.chunkPos = chunkPos;
  chunk.chunkData.width = CHUNK_BLOCKS;
  chunk.chunkData.height = CHUNK_BLOCKS;
  chunk.chunkData.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
  chunk.chunkData.mipmaps = 1;
  // 64*64*4 = 16384
  chunk.chunkData.data = calloc(chunk.chunkData.width * chunk.chunkData.height, sizeof(Color));
  chunk.chunkTexture = LoadRenderTexture(CHUNK_PIXEL, CHUNK_PIXEL);
}

void handleInput(Player &p, Camera2D &camera)
{
  p.acceleration = {0.0f, 0.0f};
  const float accel = 2000.0f;

  if (IsKeyDown(KEY_RIGHT))
    p.acceleration.x += accel;
  if (IsKeyDown(KEY_LEFT))
    p.acceleration.x -= accel;

  if (IsKeyDown(KEY_DOWN))
    p.acceleration.y += accel;
  if (IsKeyDown(KEY_UP))
    p.acceleration.y -= accel;

  if (IsKeyPressed(KEY_Q) && p.currentItem > 0)
    p.currentItem--;
  if (IsKeyPressed(KEY_E) && p.currentItem < 608)
    p.currentItem++;

  // if (IsKeyDown(KEY_UP) && p.onGround)
  //   p.velocity.y = p.jumpHeight;

  float wheel = GetMouseWheelMove();
  if (wheel != 0.0f)
  {
    camera.zoom *= 1.0f + wheel * 0.01f;
  }
}

void updatePhysics(Player &p, float dt)
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
  //
  //  update player chunk

  int x = floor(p.position.x / CHUNK_PIXEL);
  int y = floor(p.position.y / CHUNK_PIXEL);
  if (!(x < 0 || x >= 64 || y < 0 || y >= 64))
    p.currentPlayerChunk = {x, y};
}
void paintOnChunkT(Chunk &chunk)
{
  BeginTextureMode(chunk.chunkTexture);
  for (int y = 0; y < CHUNK_BLOCKS; y++)
  {
    for (int x = 0; x < CHUNK_BLOCKS; x++)
    {
      Color pixelColor = GetImageColor(chunk.chunkData, x, y);
      /*
      if (pixelColor == palette[0])
        // DrawTexture(solidsT[0], x * BLOCK_PIXEL, y * BLOCK_PIXEL, WHITE);
        DrawTextureRec(solidsT[0], (Rectangle){0, 0, BLOCK_PIXEL, BLOCK_PIXEL},
                       (Vector2){(float)x * BLOCK_PIXEL, (float)y * BLOCK_PIXEL}, WHITE);
      else if (pixelColor == palette[1])
        // DrawTexture(solidsT[1], x * BLOCK_PIXEL, y * BLOCK_PIXEL, WHITE);
        DrawTextureRec(solidsT[1], (Rectangle){0, 0, BLOCK_PIXEL, BLOCK_PIXEL},
                       (Vector2){(float)x * BLOCK_PIXEL, (float)y * BLOCK_PIXEL}, WHITE);
      else if (pixelColor == palette[2])
        // DrawTexture(solidsT[2], x * BLOCK_PIXEL, y * BLOCK_PIXEL, WHITE);
        DrawTextureRec(solidsT[2], (Rectangle){0, 0, BLOCK_PIXEL, BLOCK_PIXEL},
                       (Vector2){(float)x * BLOCK_PIXEL, (float)y * BLOCK_PIXEL}, WHITE);
      else
        cout << "Unknown color: " << pixelColor.r << ", " << pixelColor.g << ", " << pixelColor.b << endl;
        */
      uint16_t id = (pixelColor.r << 4) | (pixelColor.g >> 4);
      // cout << id << "\n";
      DrawTextureRec(solidsT[id], (Rectangle){0, 0, BLOCK_PIXEL, BLOCK_PIXEL},
                     (Vector2){(float)x * BLOCK_PIXEL, (float)y * BLOCK_PIXEL}, WHITE);
    }
  }
  EndTextureMode();
}
void draw(Player &p, array<array<unique_ptr<Chunk>, 64>, 64> &chunks, Camera2D &camera)
{
  BeginDrawing();
  ClearBackground(BlueSpace);
  BeginMode2D(camera);

  // draw chunks - goes through all chunks in chunks array in draw them
  const int dings = 6;
  for (int y = -dings; y <= dings; y++)
  {
    for (int x = -dings; x <= dings; x++)
    {
      const int chunkX = p.currentPlayerChunk.x + x;
      const int chunkY = p.currentPlayerChunk.y + y;

      if (chunkX < 0 || chunkX >= 64 || chunkY < 0 || chunkY >= 64)
        continue;

      if (!chunks[chunkY][chunkX])
        continue;

      Chunk &chunk = *chunks[chunkY][chunkX];
      // cout << chunk.chunkPos.x << ", " << chunk.chunkPos.y << "\n";
      // DrawTextureEx(
      //   chunk.texture,
      //   {(float)chunk.chunkPos.x*64, (float)chunk.chunkPos.y*64},
      //   0.0f,
      //   1,
      //   Color{255, 200, 250, 255});

      const float scale = float(CHUNK_PIXEL) / 64.0f;

      DrawTextureEx(chunk.chunkDataTexture,
                    {(float)chunk.chunkPos.x * CHUNK_PIXEL, (float)chunk.chunkPos.y * CHUNK_PIXEL}, 0.0f, 16.0f, WHITE);
    }
  }

  /* draw textures on pixels
   *  loop through all pixels in chunk and render texture for corresponding
   * pixel todo: greedy meshing
   */
  const int half_chunk_size = VIEW_RADIUS;

  for (int y = -half_chunk_size; y <= half_chunk_size; y++)
  {
    for (int x = -half_chunk_size; x <= half_chunk_size; x++)
    {
      const int chunkX = p.currentPlayerChunk.x + x;
      const int chunkY = p.currentPlayerChunk.y + y;

      if (chunkX < 0 || chunkX >= 64 || chunkY < 0 || chunkY >= 64)
        continue;

      if (chunks[chunkY][chunkX])
      {
        Chunk &chunk = *chunks[chunkY][chunkX];
        const Tuple chunkPos = {chunkX * CHUNK_PIXEL, chunkY * CHUNK_PIXEL};
        DrawTexture(chunk.chunkTexture.texture, chunkPos.x, chunkPos.y, WHITE);
        DrawTextureRec(
            chunk.chunkTexture.texture,
            (Rectangle){0, 0, (float)chunk.chunkTexture.texture.width, -(float)chunk.chunkTexture.texture.height},
            {(float)chunkPos.x, (float)chunkPos.y}, WHITE);

        /*
        // MUẞ WEG!!
        for (int y = 0; y < CHUNK_BLOCKS; y++)
        {
          for (int x = 0; x < CHUNK_BLOCKS; x++)
          {
            // GetImageColor(chunk.data, x, y);
            Color pixelColor = GetImageColor(chunk.chunkData, x, y);
            if (pixelColor == palette[0])
              DrawTexture(solidsT[0], chunkPos.x + x * BLOCK_PIXEL, chunkPos.y + y * BLOCK_PIXEL, WHITE);
            else if (pixelColor == palette[1])
              DrawTexture(solidsT[1], chunkPos.x + x * BLOCK_PIXEL, chunkPos.y + y * BLOCK_PIXEL, WHITE);
            else if (pixelColor == palette[2])
              DrawTexture(solidsT[2], chunkPos.x + x * BLOCK_PIXEL, chunkPos.y + y * BLOCK_PIXEL, WHITE);
            else
              cout << "Unknown color: " << pixelColor.r << ", " << pixelColor.g << ", " << pixelColor.b << endl;
          }
        }
        */
      }
      else
      {
        // cout << "Chunk not found";
      }
    }
  }

  // visualize chunk player is in
  DrawRectangleLinesEx({(float)p.currentPlayerChunk.x * CHUNK_PIXEL, (float)p.currentPlayerChunk.y * CHUNK_PIXEL,
                        CHUNK_PIXEL, CHUNK_PIXEL},
                       5, {255, 255, 255, 96});
  // player
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

void manageChunks(Player &p, array<array<unique_ptr<Chunk>, 64>, 64> &chunks)
{
  // load moore neighbours (more [lmao] or less) of chunk the player is in
  Tuple playerChunk = p.currentPlayerChunk;
  if (p.currentPlayerChunk.x == p.lastPlayerChunk.x && p.currentPlayerChunk.y == p.lastPlayerChunk.y)
    return;
  p.lastPlayerChunk.x = p.currentPlayerChunk.x;
  p.lastPlayerChunk.y = p.currentPlayerChunk.y;

  // chunks are loaded in a box-like structure with the witdth of CHUNK_WIDTH
  const int half_width = VIEW_RADIUS;
  for (int y = -half_width; y <= half_width; y++)
  {
    for (int x = -half_width; x <= half_width; x++)
    {
      const int chunkX = playerChunk.x + x;
      const int chunkY = playerChunk.y + y;

      if (chunkX < 0 || chunkX >= 64 || chunkY < 0 || chunkY >= 64)
        continue;

      if (!chunks[chunkY][chunkX])
      {
        // cout << "Creating chunk" << "\n";
        // Chunk tmp = initChunk(chunkY*64 + chunkX, {chunkX, chunkY});
        chunks[chunkY][chunkX] = make_unique<Chunk>();
        initChunk(*chunks[chunkY][chunkX], chunkY * 64 + chunkX, {chunkX, chunkY});
        paintChunk(*chunks[chunkY][chunkX]);
        if (chunks[chunkY][chunkX]->needsUpdate)
        {
          Chunk &chunk = *chunks[chunkY][chunkX];
          paintOnChunkT(chunk);
          chunks[chunkY][chunkX]->needsUpdate = false;
        }
      }
      else
      {
        if (chunks[chunkY][chunkX]->needsUpdate)
        {
          Chunk &chunk = *chunks[chunkY][chunkX];
          paintOnChunkT(chunk);
          chunks[chunkY][chunkX]->needsUpdate = false;
        }
      }
    }
  }

  // unload distant chunks
  const float dist = CHUNK_PIXEL * (VIEW_RADIUS + 20) * 100;
  for (int y = 0; y < chunks.size(); y++)
  {
    for (int x = 0; x < chunks[y].size(); x++)
    {
      if (chunks[y][x])
      {
        Vector2 chunkPos = {(float)x * CHUNK_PIXEL + CHUNK_PIXEL / 2, (float)y * CHUNK_PIXEL + CHUNK_PIXEL / 2};
        if (Vector2Distance(chunkPos, p.position) > dist)
        {
          // free(chunks[y][x]->data.data);
          UnloadTexture(chunks[y][x]->chunkDataTexture);
          chunks[y][x].reset();
        }
      }
    }
  }
}

int gameLoop(Player &p, array<array<unique_ptr<Chunk>, 64>, 64> &chunks, Camera2D &camera)
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
      updatePhysics(p, dt);
      accumulator -= dt;
    }

    manageChunks(p, chunks);
    camera.target = {p.position.x, p.position.y};
    draw(p, chunks, camera);
  }
  return 0;
}

int main()
{
  // initializing
  InitWindow(windowWidth, windowHeight, "elfs-and-wizards");
  SetTargetFPS(360);
  const Texture2D player_texture = LoadTexture("../assets/elf_character.png");
  Player player = {32 * 64 * 16, 32 * 64 * 16};
  player.playerTexture = player_texture;

  for (int i = 0; i < 609; i++)
  {
    string str = "../assets/solids/texture_16px ";
    str += to_string(i + 1);
    str += ".png";
    solidsT[i] = LoadTexture(str.c_str());
  }
  /*solidsT[0] = LoadTexture("../assets/solids/texture_16px 39.png");
  solidsT[1] = LoadTexture("../assets/solids/texture_16px 40.png");
  solidsT[2] = LoadTexture("../assets/solids/texture_16px 27.png");
  solidsT[3] = LoadTexture("../assets/solids/texture_16px 42.png");*/

  array<array<unique_ptr<Chunk>, 64>, 64> chunks;

  /*for (size_t i = 0; i < 10; i++)
  {
    Chunk chunk = initChunk(loadedChunks, loadedChunks.size()+1);
    paintChunk(chunk);
    /*chunk.data = GenImagePerlinNoise(
      chunk.data.width,
      chunk.data.height,
      0, 0,
      16.0f
    );
    chunk.texture = LoadTextureFromImage(chunk.data);
    loadedChunks.push_back(chunk);
  }
  */

  Camera2D camera = {0};
  camera.target = {player.position.x, player.position.y};
  camera.offset = {(1920 / 2) + 10, (1080 / 2) + 10};
  camera.rotation = 0.0f;
  camera.zoom = 1.0f;

  // start
  gameLoop(player, chunks, camera);

  CloseWindow();
  return 0;
}
