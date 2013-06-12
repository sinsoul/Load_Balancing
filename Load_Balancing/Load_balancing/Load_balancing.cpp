#include <winsock2.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "IPHLPAPI.lib")

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

typedef struct _router
{
	char pGetway[25];
	DWORD dwIF;
}RouterAdapter;

PMIB_IPFORWARDROW pRow[10];		//最多保存十条默认路由
int DRNum=0;	//默认路由的数量
RouterAdapter ra[10];	//最多十张网卡聚合
int raNum=0;


DWORD RouteAdd(char* pIP,char* pGetway,DWORD dwIF)
{
	MIB_IPFORWARDROW IpForwardTable={0};
	ZeroMemory(&IpForwardTable,sizeof(MIB_IPFORWARDROW));
	IpForwardTable.dwForwardDest=inet_addr(pIP);
	IpForwardTable.dwForwardMask=0xFF;
	IpForwardTable.dwForwardNextHop=inet_addr(pGetway);
	IpForwardTable.dwForwardIfIndex=dwIF;
	IpForwardTable.dwForwardProto=MIB_IPPROTO_NETMGMT;
	IpForwardTable.dwForwardMetric1=25;

	DWORD dwRetVal;
	if(&IpForwardTable!=NULL)
	{
		dwRetVal = CreateIpForwardEntry(&IpForwardTable);  
	}
	if (dwRetVal == NO_ERROR)
	{
		printf(".");  
	}
	else if (dwRetVal == ERROR_INVALID_PARAMETER)
	{
		printf("X");
	}
	else
	{
		printf("Error: %d\n", dwRetVal);
	}  
	return dwRetVal;
} 

DWORD RouteDelete()
{
	PMIB_IPFORWARDTABLE pIpForwardTable = NULL;
	DWORD dwSize = 0;
	BOOL bOrder = FALSE;
	DWORD dwStatus = 0;
	unsigned int i;

	dwStatus = GetIpForwardTable(pIpForwardTable, &dwSize, bOrder);
	if (dwStatus == ERROR_INSUFFICIENT_BUFFER) 
	{
		if (!(pIpForwardTable = (PMIB_IPFORWARDTABLE) malloc(dwSize))) 
		{
			printf("Malloc failed. Out of memory.\n");
		}
		dwStatus = GetIpForwardTable(pIpForwardTable, &dwSize, bOrder);
	}

	if (dwStatus != ERROR_SUCCESS) 
	{
		printf("getIpForwardTable failed.\n");
		if (pIpForwardTable)
		{
			free(pIpForwardTable);
		}
	}

	for (i = 0; i < pIpForwardTable->dwNumEntries; i++)
	{
//		printf("目标：%ld\n",pIpForwardTable->table[i].dwForwardDest);
		
		if (pIpForwardTable->table[i].dwForwardDest==10||
			pIpForwardTable->table[i].dwForwardDest==127||
			pIpForwardTable->table[i].dwForwardDest==192||
			pIpForwardTable->table[i].dwForwardDest==169||
			pIpForwardTable->table[i].dwForwardDest==172)
		{
			continue;
		}

		if (pIpForwardTable->table[i].dwForwardDest>0&&pIpForwardTable->table[i].dwForwardDest<=254)
		{
			dwStatus = DeleteIpForwardEntry(&(pIpForwardTable->table[i]));
			if (dwStatus != ERROR_SUCCESS) 
			{
				printf("Could not delete old gateway\n");
			}
			continue;
		}
	}
	if (pIpForwardTable)
	{
		free(pIpForwardTable);
	}
	return dwStatus;
}

DWORD DefaultRouteSave()
{
	PMIB_IPFORWARDTABLE pIpForwardTable = NULL;
	DWORD dwSize = 0;
	BOOL bOrder = FALSE;
	DWORD dwStatus = 0;
	unsigned int i;

	dwStatus = GetIpForwardTable(pIpForwardTable, &dwSize, bOrder);
	if (dwStatus == ERROR_INSUFFICIENT_BUFFER) 
	{
		if (!(pIpForwardTable = (PMIB_IPFORWARDTABLE) malloc(dwSize))) 
		{
			printf("Malloc failed. Out of memory.\n");
		}
		dwStatus = GetIpForwardTable(pIpForwardTable, &dwSize, bOrder);
	}

	if (dwStatus != ERROR_SUCCESS) 
	{
		printf("getIpForwardTable failed.\n");
		if (pIpForwardTable)
		{
			free(pIpForwardTable);
		}
	}

	for (i = 0; i < pIpForwardTable->dwNumEntries; i++)
	{
		if (pIpForwardTable->table[i].dwForwardDest == 0)
		{
			pRow[DRNum] = (PMIB_IPFORWARDROW) malloc(sizeof (MIB_IPFORWARDROW));
			if (!pRow) 
			{
				printf("Malloc failed. Out of memory.\n");
			}
			memcpy(pRow[DRNum], &(pIpForwardTable->table[i]),sizeof (MIB_IPFORWARDROW));
			DRNum++;
		}
	}
	if (pIpForwardTable)
	{
		free(pIpForwardTable);
	}
	printf("共有%d条默认路由\n",DRNum);
	return dwStatus;
}

void DefaultRouteRestore()
{
	DWORD dwStatus;
	int i;
	for (i=0;i<DRNum;i++)
	{
		dwStatus = CreateIpForwardEntry(pRow[i]);
		if (dwStatus == NO_ERROR)
			printf("Gateway Restore successfully\n");
		else if (dwStatus == ERROR_INVALID_PARAMETER)
			printf("Invalid parameter.\n");
		else
			printf("Error: %d\n", dwStatus);
	}
}
int __cdecl main()
{
	SetConsoleTitle(TEXT("负载均衡工具 - By SinSoul")); 
	HANDLE consolehwnd; 
	consolehwnd = GetStdHandle(STD_OUTPUT_HANDLE); 
	SetConsoleTextAttribute(consolehwnd,0xf); 

	printf("----------------------------注意--------------------------\n");
	printf("此程序会修改你机器的路由表，若出现网络故障请重启计算机\n");
	printf("如果你不知道你机器上哪些网卡能访问Internet,请直接退出程序\n");
	printf("若网关显示为0.0.0.0,请重新连接你的网络再试\n");
	printf("----------------------------------------------------------\n");
	PIP_ADAPTER_INFO pAdapterInfo;
	PIP_ADAPTER_INFO pAdapter = NULL;
	DWORD dwRetVal = 0;
	UINT i;
	char answer;
	ULONG ulOutBufLen = sizeof (IP_ADAPTER_INFO);
	pAdapterInfo = (IP_ADAPTER_INFO *) MALLOC(sizeof (IP_ADAPTER_INFO));
	if (pAdapterInfo == NULL) 
	{
		printf("Error allocating memory needed to call GetAdaptersinfo\n");
		return 1;
	}

	if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW)
	{
		FREE(pAdapterInfo);
		pAdapterInfo = (IP_ADAPTER_INFO *) MALLOC(ulOutBufLen);
		if (pAdapterInfo == NULL) 
		{
			printf("Error allocating memory needed to call GetAdaptersinfo\n");
			return 1;
		}
	}

	if ((dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) == NO_ERROR) 
	{
		pAdapter = pAdapterInfo;
		while (pAdapter)
		{
			SetConsoleTextAttribute(consolehwnd,0xa); 

			printf("网卡接口号:\t%d\n", pAdapter->Index);
			printf("网卡名称:\t%s\n", pAdapter->Description);
			printf("物理地址:\t");
			for (i = 0; i < pAdapter->AddressLength; i++) 
			{
				if (i == (pAdapter->AddressLength - 1))
				{
					printf("%.2X\n", (int) pAdapter->Address[i]);
				}
				else
				{
					printf("%.2X-", (int) pAdapter->Address[i]);
				}
			}
			printf("IP地址:\t\t%s\n",pAdapter->IpAddressList.IpAddress.String);
			printf("子网掩码:\t%s\n", pAdapter->IpAddressList.IpMask.String);
			printf("网关:\t\t%s\n\n", pAdapter->GatewayList.IpAddress.String);
			SetConsoleTextAttribute(consolehwnd,0xe); 
			printf("这张网卡能连接到Internet吗？(y/n)：");
			answer=_getch();
			if (answer=='y')
			{
				_putch(answer);
				printf("\n已经记录.\n\n");
				ZeroMemory(&ra[raNum],sizeof(RouterAdapter));
				memcpy(ra[raNum].pGetway,pAdapter->GatewayList.IpAddress.String,25);
				ra[raNum].dwIF=pAdapter->Index;
				raNum++;
			}
			else
			{
				_putch(answer);
				printf("\n不能,跳过此网卡.\n\n");
			}
			pAdapter = pAdapter->Next;
		}
	}
	else 
	{
		printf("GetAdaptersInfo failed with error: %d\n", dwRetVal);
	}
	if (pAdapterInfo)
	{
		FREE(pAdapterInfo);
	}
	if(raNum>1)
	{
		char tempip[25];

		//printf("保存默认路由\n");
		//DefaultRouteSave();

		//printf("删除默认路由\n");
		
		SetConsoleTextAttribute(consolehwnd,0xd); 
		printf("共有%d张网卡进行负载均衡，是否执行操作？(y/n):",raNum);
		answer=_getch();
		if (answer=='y')
		{
			_putch(answer);
			_putch('\n');
			RouteDelete();
			for (int i=0;i<raNum;i++)
			{
				SetConsoleTextAttribute(consolehwnd,0xa); 
				printf("\n网关地址:%s,接口序号:%d\n",ra[i].pGetway,ra[i].dwIF);
				for (int j=i;j<254;j+=raNum)
				{
					if (j==10||j==127||j==192||j==169||j==172)
					{
						continue;
					}
					ZeroMemory(&tempip,25);
					sprintf_s(tempip,"%d.0.0.0",j);
					//printf("%d,",j);
					RouteAdd(tempip,ra[i].pGetway,ra[i].dwIF);
				}
				printf("\n");
			}
			SetConsoleTitle(TEXT("最好不要关闭程序 - By SinSoul")); 
			SetConsoleTextAttribute(consolehwnd,0xf);
			printf("\n负载均衡配置完成...\n");
			SetConsoleTextAttribute(consolehwnd,0xc);
			printf("\n请不要关闭程序，按回车可恢复默认路由，取消负载均衡.\n");
			_getch();
			SetConsoleTitle(TEXT("负载均衡工具 - By SinSoul"));
			SetConsoleTextAttribute(consolehwnd,0xf);
			printf("\n开始恢复默认路由...\n");
			RouteDelete();
//			DefaultRouteRestore();
			printf("完成...\n按任意键退出程序...\n");
			_getch();
		}
		else
		{
			_putch(answer);
			SetConsoleTextAttribute(consolehwnd,0xf);
			printf("\n放弃操作，按任意键退出程序.\n");
			_getch();
		}
	}
	else if(raNum<10)
	{
		SetConsoleTextAttribute(consolehwnd,0xc);
		printf("\n只有一张网卡，不能负载均衡。\n\n");
		SetConsoleTextAttribute(consolehwnd,0xd);
		printf("如果你是想取消负载均衡，请输入y，否则任意键退出程序.(y/n):");
		answer=_getch();
		_putch(answer);
		if (answer=='y')
		{
			SetConsoleTextAttribute(consolehwnd,0xf);
			printf("\n开始恢复默认路由...\n");
			RouteDelete();
			printf("完成...\n按任意键退出程序...");
			_getch();
		}
	}
	else
	{
		SetConsoleTextAttribute(consolehwnd,0xf);
		printf("\n网卡太多，我受不了啊...\n");
	}
	return 0;
}

