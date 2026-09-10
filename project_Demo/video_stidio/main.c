#include "app/studio.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char** argv)
{
	int frames = -1;
	Studio* s = studio_create();
	if (!s)
	{
		fprintf(stderr, "video_stidio 初始化失败\n");
		return 1;
	}
	for (int i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "--frames") && i + 1 < argc)
			frames = atoi(argv[++i]);
		else
			studio_import(s, argv[i]);
	}
	while (frames != 0 && studio_pump(s))
	{
		studio_tick(s);
		SDL_Delay(8);
		if (frames > 0)
			frames--;
	}
	studio_destroy(s);
	return 0;
}
