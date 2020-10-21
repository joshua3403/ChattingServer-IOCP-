#include "stdafx.h"
#include "ChattingServer.h"

int main()
{
	ChattingServer* test = new ChattingServer();
	LOG_SET(LOG_CONSOLE | LOG_FILE, LOG_DEBUG);

	setlocale(LC_ALL, "");


	test->Start(12001, TRUE, NULL, 8, 10000);

	while (true)
	{

		Sleep(1000);
	}
}