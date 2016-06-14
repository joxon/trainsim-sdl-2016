#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <conio.h>
#include <windows.h>
#include <process.h>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include "output.h"
#include "state_trans.h"
#include "control.h"
#include "drawSDL.h"

int trainNum;                                 //火车数量
int railNum;                                  //轨道数量
struct train train[MAX_TRAIN];                         //火车上限
float        trainSpeed[MAX_TRAIN];                    //火车速度
struct block railway[MAX_RAIL][MAX_RAIL_LENGTH];       //轨道与长度上限

int strategy = 0; //1，2，3分别对应"交替策略"，"快车优先策略"，"人工控制"
int inputMode = FROM_KEYBOARD; //1，2分别对应"从文件读入命令"，"从键盘读入命令"
unsigned int processTime = 0; //程序时钟
unsigned int commandTime = 0;
char         command;

FILE *logPtr = NULL; //日志文件指针
FILE *commandPtr = NULL; //命令文件指针
FILE *outPtr = NULL; //输出文件指针



//void errorFromFile();
void initFromFile();

SDL_Rect clip[BLOCK_ROW][BLOCK_COLUMN];

int main(int argc, char* args[])
{
	//初始化火车和轨道
	//initFromFile();

	//SDL初始化
	SDL_Init(SDL_INIT_VIDEO);
	IMG_Init(IMG_INIT_PNG);
	TTF_Init();

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
	SDL_Window* window = SDL_CreateWindow("Trainsim", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	TTF_Font* font = TTF_OpenFont("font.ttf", 30);
	SDL_Texture* blocksTexture = IMG_LoadTexture(renderer, "blocks.png");

	for (int i = 0; i < BLOCK_ROW; ++i)
		for (int j = 0; j < BLOCK_COLUMN; ++j)
		{
			clip[i][j].x = i*BLOCK_SIZE;
			clip[i][j].y = j*BLOCK_SIZE;
			clip[i][j].w = BLOCK_SIZE;
			clip[i][j].h = BLOCK_SIZE;
		}

	//火车和轨道界面
	SDL_Rect trainViewport;
	trainViewport.x = 0;
	trainViewport.y = 0;
	trainViewport.h = WINDOW_HEIGHT;
	trainViewport.w = WINDOW_WIDTH * 5 / 6;

	//用户输入界面
	SDL_Rect userViewport;
	userViewport.x = WINDOW_WIDTH * 5 / 6;
	userViewport.y = 0;
	userViewport.h = WINDOW_HEIGHT;
	userViewport.w = WINDOW_WIDTH / 6;

	//主循环
	bool quit = false;
	SDL_Event e;
	while (!quit)
	{
		while (SDL_PollEvent(&e))
		{
			if (e.type == SDL_QUIT) quit = true;
		}

		SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
		SDL_RenderClear(renderer);

		int i;
		//控制命令
		if (inputMode == FROM_FILE)
		{
			for (i = 0; i < trainNum; ++i)
				if (processTime == train[i].startTime)
					train[i].speed = trainSpeed[i];     //到启动时刻返还速度
			getInputFromFile();
		}
		else if (inputMode == FROM_KEYBOARD)
			for (i = 0; i < trainNum; ++i)
				if (processTime == train[i].startTime)
					train[i].speed = trainSpeed[i];     //到启动时刻返还速度

														//状态变换
		for (i = 0; i < trainNum; ++i)
			trans(&train[i], railway, i);

		//输出
		print();

		//火车移动
		for (i = 0; i < trainNum; ++i)
		{
			changeDirection(&train[i], railway, i);
			changePosition(&train[i]);
		}

		//渲染轨道和火车
		SDL_RenderSetViewport(renderer, &trainViewport);
		drawRailway();
		drawTrain();

		//渲染用户输入界面
		SDL_RenderSetViewport(renderer, &userViewport);
		drawUI();

		SDL_RenderPresent(renderer);



		////时间片推进
		//++processTime;
	}

	//释放资源
	TTF_CloseFont(font);
	SDL_DestroyTexture(blocksTexture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	TTF_Quit();
	IMG_Quit();
	SDL_Quit();

	return 0;
}

//void errorFromFile()
//{
//	/*文件输入错误*/
//	system("cls");
//	printf("ERROR data detected! Please check init.txt.\n"
//		"Press any key to exit...");
//	_getch();
//	exit(EXIT_FAILURE);
//}

void initFromFile()
{
	FILE *fp;
	int id;
	char ch;

	fp = fopen(".\\txt\\init.txt", "r");
	if (fp != NULL)
	{
		/*策略选择*/
		fscanf(fp, "strategy=%d\n", &strategy);
		//if (strategy != ALTERNATIVE ||
		//	strategy != FAST_FIRST ||
		//	strategy != MANUAL)
		//	errorFromFile();

		/*火车初始化*/
		fscanf(fp, "train.num=%d\n", &trainNum);
		//if (trainNum < 0) errorFromFile();
		for (id = 0; id < trainNum; ++id)
		{
			fscanf(fp, "train%c.speed=%f st=%d sp=%d dir=%d type=%d pt=%d\n",
				&ch, &trainSpeed[id], &train[id].startTime, &train[id].startPoint,
				&train[id].direction, &train[id].type, &train[id].pausetime);
			//if (train[id].speed < 0 || train[id].startTime < 0 || train[id].startPoint < 0 ||
			//	train[id].direction != NORMAL || train[id].direction != REVERSE ||
			//	train[id].type != FAST || train[id].type != SLOW || train[id].pausetime < 0)
			//	errorFromFile();
			train[id].position = train[id].startPoint;
			train[id].status = WAIT;
		}


		/*轨道初始化*/
		fscanf(fp, "railway.num=%d\n", &railNum);
		//if (railNum < 0) errorFromFile();
		for (id = 0; id < railNum; ++id)
		{
			/*轨道初始化：长宽设定*/
			int length, southwest, northwest, northeast, southeast;
			fscanf(fp, "railway%c.len=%d sw=%d nw=%d ne=%d se=%d\n",
				&ch, &length, &southwest, &northwest, &northeast, &southeast);
			//if (length < 0 || southeast < 0 || northeast < 0 || northwest < 0 || southeast < 0) errorFromFile();
			train[id].railwayLength = length;
			railway[id][southwest].direction = SOUTHWEST;
			railway[id][northwest].direction = NORTHWEST;
			railway[id][northeast].direction = NORTHEAST;
			railway[id][southeast].direction = SOUTHEAST;
			int blockid = 0;
			for (blockid = southwest + 1; blockid < northwest; ++blockid)
			{
				railway[id][blockid].direction = WEST;
				railway[id][blockid].station = 0;
			}
			for (blockid = northwest + 1; blockid < northeast; ++blockid)
			{
				railway[id][blockid].direction = NORTH;
				railway[id][blockid].station = 0;
			}
			for (blockid = northeast + 1; blockid < southeast; ++blockid)
			{
				railway[id][blockid].direction = EAST;
				railway[id][blockid].station = 0;
			}
			for (blockid = southeast + 1; blockid < length; ++blockid)
			{
				railway[id][blockid].direction = SOUTH;
				railway[id][blockid].station = 0;
			}

			/*轨道初始化：公共轨道*/
			int common_count, common_ID, start, end;
			fscanf(fp, "cm=%d\n", &common_count);
			//if (common_count < 0) errorFromFile();
			int commomid = 1;
			while (commomid++ <= common_count)
			{
				fscanf(fp, "id=%d %d %d\n", &common_ID, &start, &end);
				//if (common_count <= 0 || start < 0 || end < 0) errorFromFile();
				for (blockid = start; blockid < end; ++blockid)
				{
					railway[id][blockid].common = common_ID;
					railway[id][blockid].last = -1;
				}
			}

			/*轨道初始化：停靠点*/
			int stationCount, stationID = 1, stationPoint;
			fscanf(fp, "sn=%d\n", &stationCount);
			//if (stationCount < 0) errorFromFile();
			while (stationID++ <= stationCount)
			{
				fscanf(fp, "%d\n", &stationPoint);
				//if (stationPoint < 0) errorFromFile();
				railway[id][stationPoint].station = 1;
			}

			//复制轨道起始部分，防止越界访问，i的范围视速度而定
			for (int i = 0; i < 10; ++i)
				railway[id][length + i] = railway[id][i];
		}
		fclose(fp);
		printf("init successful!");
	}

	else
	{
		printf("Cannot open init.txt. Press any key to exit...");
		_getch();
		exit(EXIT_FAILURE);
	}
}