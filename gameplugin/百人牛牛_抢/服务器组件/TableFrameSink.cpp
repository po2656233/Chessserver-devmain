#include "StdAfx.h"
#include "DlgCustomRule.h"
#include "TableFrameSink.h"
#include <locale>

//////////////////////////////////////////////////////////////////////////////////
//常量定义
#define SEND_COUNT					100									//发送次数

//索引定义
#define INDEX_PLAYER				0									//闲家索引
#define INDEX_BANKER				1									//庄家索引

//时间定义
#define IDI_FREE					1									//空闲时间
#define IDI_PLACE_JETTON			2									//下注时间
#define IDI_GAME_END				3									//结束时间

#define TIME_FREE					5									//空闲时间
#define TIME_PLACE_JETTON			10									//下注时间
#define TIME_GAME_END				20									//结束时间
//////////////////////////////////////////////////////////////////////////

//静态变量
const WORD			CTableFrameSink::m_wPlayerCount=GAME_PLAYER;				//游戏人数

void Debug(char *text,...)
{
	static DWORD num=0;	
	char buf[1024];	
	FILE *fp=NULL;	
	va_list ap;	
	va_start(ap,text);	
	vsprintf(buf,text,ap);	
	va_end(ap);		
	if(num == 0)	
	{		
		fp=fopen("百人牛牛库存.log","w");	
	}	
	else	
	{		
		fp=fopen("百人牛牛库存.log","a");	
	}	
	if(fp == NULL)	
		return ;	
	num++;	
	SYSTEMTIME time;	
	GetLocalTime(&time);	
	fprintf(fp, "%d:%s - %d/%d/%d %d:%d:%d \n", num, buf, time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);	
	fclose(fp); 
}
//////////////////////////////////////////////////////////////////////////
//构造函数
CTableFrameSink::CTableFrameSink()
{
	//起始分数
	ZeroMemory(m_lUserStartScore,sizeof(m_lUserStartScore));

	//总下注数
	ZeroMemory(m_lAllJettonScore,sizeof(m_lAllJettonScore));

	//个人下注
	ZeroMemory(m_lUserJettonScore,sizeof(m_lUserJettonScore));

	//控制变量
	m_cbWinSideControl=0;
	m_nSendCardCount=0;
	m_bControl=false;

	//玩家成绩	
	ZeroMemory(m_lUserWinScore,sizeof(m_lUserWinScore));
	ZeroMemory(m_lUserReturnScore,sizeof(m_lUserReturnScore));
	ZeroMemory(m_lUserRevenue,sizeof(m_lUserRevenue));

	//扑克信息
	ZeroMemory(m_cbCardCount,sizeof(m_cbCardCount));
	ZeroMemory(m_cbTableCardArray,sizeof(m_cbTableCardArray));

	//状态变量
	m_dwJettonTime=0L;
	m_bExchangeBanker=true;
	m_wAddTime=0;
	CopyMemory(m_szControlName,TEXT("无人"),sizeof(m_szControlName));

	//庄家信息
	m_ApplyUserArray.RemoveAll();
	m_wCurrentBanker=INVALID_CHAIR;
	m_wOfflineBanker = INVALID_CHAIR;
	m_wBankerTime=0;
	m_lPlayerBankerMAX = 0l;
	m_lBankerWinScore=0L;		
	m_lBankerCurGameScore=0L;
	m_bEnableSysBanker=true;
	m_cbLeftCardCount=0;
	m_bContiueCard=false;
	m_lBankerScore = 0l;

	//记录变量
	ZeroMemory(m_GameRecordArrary,sizeof(m_GameRecordArrary));
	m_nRecordFirst=0;
	m_nRecordLast=0;
	m_dwRecordCount=0;

	//机器人控制
	m_nChipRobotCount = 0;
	m_nRobotListMaxCount =0;
	ZeroMemory(m_lRobotAreaScore, sizeof(m_lRobotAreaScore));
	
	ZeroMemory(&m_superbankerConfig, sizeof(m_superbankerConfig));
	m_typeCurrentBanker = INVALID_SYSBANKER;
	m_wCurSuperRobBankerUser = INVALID_CHAIR;

	ZeroMemory(&m_occupyseatConfig, sizeof(m_occupyseatConfig));

	//占位
	for (WORD i=0; i<MAX_OCCUPY_SEAT_COUNT; i++)
	{
		m_wOccupySeatChairIDArray[i] = INVALID_CHAIR;
	}

	//服务控制
	m_hInst = NULL;
	m_pServerContro = NULL;
	m_hInst = LoadLibrary(TEXT("OxBattleServerControl.dll"));
	if ( m_hInst )
	{
		typedef void * (*CREATE)(); 
		CREATE ServerControl = (CREATE)GetProcAddress(m_hInst,"CreateServerControl"); 
		if ( ServerControl )
		{
			m_pServerContro = static_cast<IServerControl*>(ServerControl());
		}
	}

	return;
}

//析构函数
CTableFrameSink::~CTableFrameSink()
{
}

VOID CTableFrameSink::Release()
{
	//if( m_pServerContro )
	//{
	//	delete m_pServerContro;
	//	m_pServerContro = NULL;
	//}

	if( m_hInst )
	{
		FreeLibrary(m_hInst);
		m_hInst = NULL;
	}
	delete this; 
}
//接口查询
VOID * CTableFrameSink::QueryInterface(REFGUID Guid, DWORD dwQueryVer)
{
	QUERYINTERFACE(ITableFrameSink,Guid,dwQueryVer);
	QUERYINTERFACE(ITableUserAction,Guid,dwQueryVer);
	QUERYINTERFACE_IUNKNOWNEX(ITableFrameSink,Guid,dwQueryVer);
	return NULL;
}

//配置桌子
bool CTableFrameSink::Initialization(IUnknownEx * pIUnknownEx)
{
	//查询接口
	m_pITableFrame=QUERY_OBJECT_PTR_INTERFACE(pIUnknownEx,ITableFrame);

	//错误判断
	if (m_pITableFrame==NULL)
	{
		CTraceService::TraceString(TEXT("游戏桌子 CTableFrameSink 查询 ITableFrame 接口失败"),TraceLevel_Exception);
		return false;
	}

	//开始模式
	m_pITableFrame->SetStartMode(START_MODE_TIME_CONTROL);

	m_pGameServiceOption = m_pITableFrame->GetGameServiceOption();
	m_pGameServiceAttrib = m_pITableFrame->GetGameServiceAttrib();

	ASSERT(m_pGameServiceOption!=NULL);
	ASSERT(m_pGameServiceAttrib!=NULL);
	//设置文件名
	TCHAR szPath[MAX_PATH]=TEXT("");
	GetCurrentDirectory(sizeof(szPath),szPath);
	_sntprintf(m_szConfigFileName,sizeof(m_szConfigFileName),TEXT("%s\\OxBattleConfig.ini"),szPath);

	//房间名称
	memcpy(m_szRoomName, m_pGameServiceOption->szServerName, sizeof(m_pGameServiceOption->szServerName));

	ReadConfigInformation();

	return true;
}

//复位桌子
VOID CTableFrameSink::RepositionSink()
{
	m_wOfflineBanker = INVALID_CHAIR;

	//总下注数
	ZeroMemory(m_lAllJettonScore,sizeof(m_lAllJettonScore));

	//个人下注
	ZeroMemory(m_lUserJettonScore,sizeof(m_lUserJettonScore));

	//玩家成绩	
	ZeroMemory(m_lUserWinScore,sizeof(m_lUserWinScore));
	ZeroMemory(m_lUserReturnScore,sizeof(m_lUserReturnScore));
	ZeroMemory(m_lUserRevenue,sizeof(m_lUserRevenue));

	//机器人控制
	m_nChipRobotCount = 0;
	m_bControl=false;
	ZeroMemory(m_lRobotAreaScore, sizeof(m_lRobotAreaScore));
	
	m_lBankerScore = 0l;

	return;
}

//查询限额
SCORE CTableFrameSink::QueryConsumeQuota(IServerUserItem * pIServerUserItem)
{
	if(pIServerUserItem->GetUserStatus() == US_PLAYING)
	{
		return 0L;
	}
	else
	{
		return __max(pIServerUserItem->GetUserScore()-m_pGameServiceOption->lMinTableScore, 0L);
	}
}

//积分事件
bool CTableFrameSink::OnUserScroeNotify(WORD wChairID, IServerUserItem * pIServerUserItem, BYTE cbReason)
{
	//当庄家分数在空闲时间变动时(即庄家进行了存取款)校验庄家的上庄条件
	if(wChairID == m_wCurrentBanker && m_pITableFrame->GetGameStatus()==GAME_SCENE_FREE)
	{
		ChangeBanker(false);
	}
	
	//校验是否满足占位最低条件
	if (pIServerUserItem->GetUserScore() < m_occupyseatConfig.lForceStandUpCondition)
	{
		bool bvalid = false;
		//校验数据
		for (WORD i=0; i<MAX_OCCUPY_SEAT_COUNT; i++)
		{
			if (m_wOccupySeatChairIDArray[i] == wChairID)
			{
				bvalid = true;

				//重置无效
				m_wOccupySeatChairIDArray[i] = INVALID_CHAIR;

				break;
			}
		}

		if (bvalid == true)
		{
			CMD_S_UpdateOccupySeat UpdateOccupySeat;
			ZeroMemory(&UpdateOccupySeat, sizeof(UpdateOccupySeat));
			CopyMemory(UpdateOccupySeat.wOccupySeatChairIDArray, m_wOccupySeatChairIDArray, sizeof(m_wOccupySeatChairIDArray));
			UpdateOccupySeat.wQuitOccupySeatChairID = wChairID;

			//发送数据
			m_pITableFrame->SendTableData(INVALID_CHAIR, SUB_S_UPDATE_OCCUPYSEAT, &UpdateOccupySeat, sizeof(UpdateOccupySeat));
			m_pITableFrame->SendLookonData(INVALID_CHAIR, SUB_S_UPDATE_OCCUPYSEAT, &UpdateOccupySeat, sizeof(UpdateOccupySeat));
		}

	}

	return true;
}

//游戏开始
bool CTableFrameSink::OnEventGameStart()
{
	//设置随机种子
	srand(GetTickCount());

	//发送库存消息
	for (WORD i=0;i<GAME_PLAYER;i++)
	{
		//获取用户
		IServerUserItem * pIServerUserItem=m_pITableFrame->GetTableUserItem(i);
		if ( pIServerUserItem == NULL )
			continue;

		if( CUserRight::IsGameCheatUser(m_pITableFrame->GetTableUserItem(i)->GetUserRight()))
		{
			CString strInfo;
			strInfo.Format(TEXT("当前库存：%I64d"), m_lStorageCurrent);

			m_pITableFrame->SendGameMessage(pIServerUserItem,strInfo,SMT_CHAT);
		}	
	}
	
	CString strStorage;
	CTime Time(CTime::GetCurrentTime());
	strStorage.Format(TEXT(" 房间: %s | 时间: %d-%d-%d %d:%d:%d | 库存: %I64d \n"), m_pGameServiceOption->szServerName, Time.GetYear(), Time.GetMonth(), Time.GetDay(), Time.GetHour(), Time.GetMinute(), Time.GetSecond(), m_lStorageCurrent );

	CString strFileName;
	strFileName.Format(TEXT("百人牛牛[%s]库存日志.log"), m_pGameServiceOption->szServerName);
	WriteInfo(strFileName, strStorage);
	
	//变量定义
	CMD_S_GameStart GameStart;
	ZeroMemory(&GameStart,sizeof(GameStart));

	//获取庄家
	IServerUserItem* pIBankerServerUserItem = NULL;
	if ( m_wCurrentBanker == INVALID_CHAIR )
	{
		m_lBankerScore = 1000000000;
	}
	else
	{
		IServerUserItem* pIBankerServerUserItem = m_pITableFrame->GetTableUserItem(m_wCurrentBanker);
		if ( pIBankerServerUserItem != NULL )
		{
			m_lBankerScore = pIBankerServerUserItem->GetUserScore();
		}
	}

	//设置变量
	GameStart.cbTimeLeave=m_nPlaceJettonTime;
	GameStart.wBankerUser=m_wCurrentBanker;
	GameStart.lBankerScore=m_lBankerScore;
	GameStart.bContiueCard=m_bContiueCard;
	
	CopyMemory(GameStart.szServerName, m_pGameServiceOption->szServerName, sizeof(GameStart.szServerName));
	

	//下注机器人数量
	int nChipRobotCount = 0;
	for (int i = 0; i < GAME_PLAYER; i++)
	{
		IServerUserItem *pIServerUserItem=m_pITableFrame->GetTableUserItem(i);
		if (pIServerUserItem != NULL && pIServerUserItem->IsAndroidUser())
			nChipRobotCount++;
	}

	GameStart.nChipRobotCount = min(nChipRobotCount, m_nMaxChipRobot);
	
	nChipRobotCount = 0;
	for (int i = 0; i < m_ApplyUserArray.GetCount(); i++) 
	{
		IServerUserItem *pIServerUserItem=m_pITableFrame->GetTableUserItem(m_ApplyUserArray.GetAt(i));
		if (pIServerUserItem != NULL && pIServerUserItem->IsAndroidUser())
			nChipRobotCount++;
	}

	if(nChipRobotCount > 0)
		GameStart.nAndriodApplyCount=nChipRobotCount-1;

	//机器人控制
	m_nChipRobotCount = 0;
	ZeroMemory(m_lRobotAreaScore, sizeof(m_lRobotAreaScore));

	//旁观玩家
	m_pITableFrame->SendLookonData(INVALID_CHAIR,SUB_S_GAME_START,&GameStart,sizeof(GameStart));	

	//游戏玩家
	for (WORD wChairID=0; wChairID<GAME_PLAYER; ++wChairID)
	{
		IServerUserItem *pIServerUserItem=m_pITableFrame->GetTableUserItem(wChairID);
		if (pIServerUserItem==NULL) continue;
		int iTimer = 10;

		//设置积分
		GameStart.lUserMaxScore=min(pIServerUserItem->GetUserScore(),m_lUserLimitScore*iTimer);

		m_pITableFrame->SendTableData(wChairID,SUB_S_GAME_START,&GameStart,sizeof(GameStart));	
	}

	return true;
}

//游戏结束
bool CTableFrameSink::OnEventGameConclude(WORD wChairID, IServerUserItem * pIServerUserItem, BYTE cbReason)
{
	switch (cbReason)
	{
	case GER_NORMAL:		//常规结束	
		{
			//游戏作弊
			if( m_pServerContro != NULL && m_pServerContro->NeedControl() )
			{
				//获取控制目标牌型
				m_bControl=true;
				m_pServerContro->GetSuitResult(m_cbTableCardArray, m_cbTableCard, m_lAllJettonScore);
				m_pServerContro->CompleteControl();
			}
			else
			{	
				//系统坐庄和机器人坐庄优化库存
				if ((m_wCurrentBanker == INVALID_CHAIR && m_bEnableSysBanker == true)
				 || (m_wCurrentBanker != INVALID_CHAIR && m_pITableFrame->GetTableUserItem(m_wCurrentBanker)->IsAndroidUser() == true))
				{
					StorageOptimize();
				}	
				else if (m_wCurrentBanker != INVALID_CHAIR && m_pITableFrame->GetTableUserItem(m_wCurrentBanker)->IsAndroidUser() == false)
				{
					//循环计数
					LONGLONG lCirculateCount = 0L;
					while(true)
					{
						if (lCirculateCount > 10000)
						{
							break;
						}

						//真人玩家得分
						LONGLONG lRealPlayerWinScore = CalRealPlayerBankerScore();

						//真人玩家输钱
						if (lRealPlayerWinScore <= 0)
						{
							break;
						}
						else		//真人玩家赢钱
						{
							if (m_lStorageCurrent - lRealPlayerWinScore  <= 0)
							{
								DispatchTableCard();
								lCirculateCount++;
								continue;
							}

							//检验最大赔付比例
							LONGLONG lStorageCurrent = m_lStorageCurrent - lRealPlayerWinScore;
							double llCompensateRadio = (double)((double)30 / (double)100);
							if (m_lStorageCurrent * (1 - llCompensateRadio) >= lStorageCurrent)
							{
								DispatchTableCard();
								lCirculateCount++;
								continue;
							}

							//判断条件
							if (((m_lStorageCurrent > m_lStorageMax1) && (rand()%100 < m_lStorageMul1))
								|| ((m_lStorageCurrent > m_lStorageMax2) && (rand()%100 < m_lStorageMul2))
								|| rand() % 100 < 45)
							{
								break;
							}
							else
							{
								DispatchTableCard();
								lCirculateCount++;
								continue;
							}
						}
					}
					
				}
			}

			//计算分数
			LONGLONG lBankerWinScore=CalculateScore();

			//递增次数
			m_wBankerTime++;

			//结束消息
			CMD_S_GameEnd GameEnd;
			ZeroMemory(&GameEnd,sizeof(GameEnd));

			//占位玩家成绩
			for (WORD i=0; i<MAX_OCCUPY_SEAT_COUNT; i++)
			{
				if (m_wOccupySeatChairIDArray[i] == INVALID_CHAIR)
				{
					continue;
				}
				IServerUserItem *pIOccupySeatServerUserItem = m_pITableFrame->GetTableUserItem(m_wOccupySeatChairIDArray[i]);
				if (!pIOccupySeatServerUserItem)
				{
					continue;
				}

				GameEnd.lOccupySeatUserWinScore[i] = m_lUserWinScore[m_wOccupySeatChairIDArray[i]];
			}

			//庄家信息
			GameEnd.nBankerTime = m_wBankerTime;
			GameEnd.lBankerTotallScore=m_lBankerWinScore;
			GameEnd.lBankerScore=lBankerWinScore;
			GameEnd.bcFirstCard = m_bcFirstPostCard;

			//扑克信息
			CopyMemory(GameEnd.cbTableCardArray,m_cbTableCardArray,sizeof(m_cbTableCardArray));
			GameEnd.cbLeftCardCount=m_cbLeftCardCount;

			//发送积分
			GameEnd.cbTimeLeave=m_nGameEndTime;	
			for ( WORD wUserIndex = 0; wUserIndex < GAME_PLAYER; ++wUserIndex )
			{
				IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(wUserIndex);
				if ( pIServerUserItem == NULL ) continue;

				//设置成绩
				GameEnd.lUserScore=m_lUserWinScore[wUserIndex];

				//返还积分
				GameEnd.lUserReturnScore=m_lUserReturnScore[wUserIndex];

				//设置税收
				if (m_lUserRevenue[wUserIndex]>0) GameEnd.lRevenue=m_lUserRevenue[wUserIndex];
				else if (m_wCurrentBanker!=INVALID_CHAIR) GameEnd.lRevenue=m_lUserRevenue[m_wCurrentBanker];
				else GameEnd.lRevenue=0;

				//发送消息					
				m_pITableFrame->SendTableData(wUserIndex,SUB_S_GAME_END,&GameEnd,sizeof(GameEnd));
				m_pITableFrame->SendLookonData(wUserIndex,SUB_S_GAME_END,&GameEnd,sizeof(GameEnd));
			}
		
			CString strJetton;
			CString strArea;
			for (WORD i=0; i<GAME_PLAYER; i++)
			{
				strJetton = TEXT("");
				
				IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(i);
				if (pIServerUserItem == NULL) 
				{
					continue;
				}
				if (pIServerUserItem->IsAndroidUser() == true)
				{
					continue;
				}
				
				bool bJetton = false;
				for (WORD wAreaIndex=1; wAreaIndex<AREA_COUNT+1; wAreaIndex++)
				{
					strArea = TEXT("");
					if (m_lUserJettonScore[wAreaIndex][i] == 0)
					{
						continue;
					}
					switch(wAreaIndex)
					{
					case 1:
						{
							strArea.Format(TEXT("东 ：%I64d "), m_lUserJettonScore[wAreaIndex][i]);
							break;
						}
					case 2:
						{
							strArea.Format(TEXT("南 ：%I64d "), m_lUserJettonScore[wAreaIndex][i]);
							break;
						}
					case 3:
						{
							strArea.Format(TEXT("西 ：%I64d "), m_lUserJettonScore[wAreaIndex][i]);
							break;
						}
					case 4:
						{
							strArea.Format(TEXT("北 ：%I64d "), m_lUserJettonScore[wAreaIndex][i]);
							break;
						}
					default:
						ASSERT(false);
					}
					
					strJetton += strArea;
					bJetton = true;
				}
				
				if (bJetton == true)
				{
					CString strControlInfo;
					CTime Time(CTime::GetCurrentTime());
					strControlInfo.Format(TEXT("房间: %s | 时间: %d-%d-%d %d:%d:%d\n 昵称: %s --得分为:%I64d  ////"),
						m_pGameServiceOption->szServerName, Time.GetYear(), Time.GetMonth(), Time.GetDay(),
						Time.GetHour(), Time.GetMinute(), Time.GetSecond(), pIServerUserItem->GetNickName(), m_lUserWinScore[i]);
					
					strControlInfo += strJetton;
					strControlInfo += TEXT("\n");

					CString strFileName;
					strFileName.Format(TEXT("百人牛牛[%s]下注 信息.log"), m_pGameServiceOption->szServerName);

					WriteInfo(strFileName, strControlInfo);
				}

			}
			
			if (NeedDeductStorage())
			{
				CString strtype;
				CString strtip;
				CString strCardInfo;
				BYTE cbCardType[5];
				ZeroMemory(cbCardType, sizeof(cbCardType));
				for(int i = 0;i<AREA_COUNT+1;i++) 
				{
					strtype = TEXT("");
					BYTE bcTmp[5];
					cbCardType[i] = m_GameLogic.GetCardType(m_cbTableCardArray[i],5,bcTmp);
					strCardInfo = TEXT("");
					switch(i)
					{
					case 0:
						{
							strtype.Format(TEXT("庄【%d】//"), cbCardType[i]);
							for (WORD wCardIndex=0; wCardIndex<5; wCardIndex++)
							{
								strtype += GetCardInfo(m_cbTableCardArray[i][wCardIndex]);
							}
							strtype += TEXT("////////");
							break;
						}
					case 1:
						{
							strtype.Format(TEXT("东【%d】//"), cbCardType[i]);
							for (WORD wCardIndex=0; wCardIndex<5; wCardIndex++)
							{
								strtype += GetCardInfo(m_cbTableCardArray[i][wCardIndex]);
							}
							strtype += TEXT("////////");
							break;
						}
					case 2:
						{
							strtype.Format(TEXT("南【%d】//"), cbCardType[i]);
							for (WORD wCardIndex=0; wCardIndex<5; wCardIndex++)
							{
								strtype += GetCardInfo(m_cbTableCardArray[i][wCardIndex]);
							}
							strtype += TEXT("////////");
							break;
						}
					case 3:
						{
							strtype.Format(TEXT("西【%d】//"), cbCardType[i]);
							for (WORD wCardIndex=0; wCardIndex<5; wCardIndex++)
							{
								strtype += GetCardInfo(m_cbTableCardArray[i][wCardIndex]);
							}
							strtype += TEXT("////////");
							break;
						}
					case 4:
						{
							strtype.Format(TEXT("北【%d】//"), cbCardType[i]);
							for (WORD wCardIndex=0; wCardIndex<5; wCardIndex++)
							{
								strtype += GetCardInfo(m_cbTableCardArray[i][wCardIndex]);
							}
							strtype += TEXT("////////");
							break;
						}
					default:
						ASSERT(false);
					}
					
					strtip += strtype;
				}
				
				CString strFileName;
				strFileName.Format(TEXT("百人牛牛[%s]下注 信息.log"), m_pGameServiceOption->szServerName);

				WriteInfo(strFileName, strtip);

				if ((m_wCurrentBanker == INVALID_CHAIR && m_bEnableSysBanker == true)
					|| (m_wCurrentBanker != INVALID_CHAIR && m_pITableFrame->GetTableUserItem(m_wCurrentBanker)->IsAndroidUser() == true))
				{

					CString strControlInfo;
					strControlInfo.Format(TEXT("当局系统或者机器人坐庄，真人得分为 = %I64d \n"), CalSysOrAndroidBankerScore());
					
					WriteInfo(strFileName, strControlInfo);
					WriteInfo(strFileName, TEXT("\n"));
				}	
				else if (m_wCurrentBanker != INVALID_CHAIR && m_pITableFrame->GetTableUserItem(m_wCurrentBanker)->IsAndroidUser() == false)
				{
					CString strControlInfo;
					strControlInfo.Format(TEXT("当局真人坐庄，真人得分为 = %I64d \n"), CalRealPlayerBankerScore());

					WriteInfo(strFileName, strControlInfo);
					WriteInfo(strFileName, TEXT("\n"));
				}


			}
			
			return true;
		}
	case GER_USER_LEAVE:		//用户离开
	case GER_NETWORK_ERROR:
		{
			if (wChairID == m_wCurSuperRobBankerUser)
			{
				m_wCurSuperRobBankerUser = INVALID_CHAIR;

				CMD_S_CurSuperRobLeave CurSuperRobLeave;
				ZeroMemory(&CurSuperRobLeave,sizeof(CurSuperRobLeave));

				//设置变量
				CurSuperRobLeave.wCurSuperRobBankerUser = m_wCurSuperRobBankerUser;

				//发送消息
				m_pITableFrame->SendTableData(INVALID_CHAIR, SUB_S_CURSUPERROB_LEAVE, &CurSuperRobLeave, sizeof(CurSuperRobLeave));
				m_pITableFrame->SendLookonData(INVALID_CHAIR, SUB_S_CURSUPERROB_LEAVE, &CurSuperRobLeave, sizeof(CurSuperRobLeave));
			}

			//闲家判断
			if (m_wCurrentBanker!=wChairID)
			{
				//变量定义
				LONGLONG lRevenue=0;

				//是否开牌
				if (m_pITableFrame->GetGameStatus() == GAME_SCENE_PLACE_JETTON)
				{
					//返回下注
					for (int nAreaIndex=1; nAreaIndex<=AREA_COUNT; ++nAreaIndex) 
					{
						if (m_lUserJettonScore[nAreaIndex][wChairID] != 0)
						{
							CMD_S_PlaceJettonFail PlaceJettonFail;
							ZeroMemory(&PlaceJettonFail,sizeof(PlaceJettonFail));
							PlaceJettonFail.lJettonArea=nAreaIndex;
							PlaceJettonFail.lPlaceScore=m_lUserJettonScore[nAreaIndex][wChairID];
							PlaceJettonFail.wPlaceUser=wChairID;

							//游戏玩家
							for (WORD i=0; i<GAME_PLAYER; ++i)
							{
								IServerUserItem *pIServerUserItem=m_pITableFrame->GetTableUserItem(i);
								if (pIServerUserItem==NULL) 
									continue;

								m_pITableFrame->SendTableData(i,SUB_S_PLACE_JETTON_FAIL,&PlaceJettonFail,sizeof(PlaceJettonFail));
							}

							m_lAllJettonScore[nAreaIndex] -= m_lUserJettonScore[nAreaIndex][wChairID];
							m_lUserJettonScore[nAreaIndex][wChairID] = 0;
						}
					}
				}
				else
				{
					//写入积分
					if (m_lUserWinScore[wChairID]!=0L) 
					{
						tagScoreInfo ScoreInfo[GAME_PLAYER];
						ZeroMemory(ScoreInfo,sizeof(ScoreInfo));
						//库存金币
						if (!pIServerUserItem->IsAndroidUser())
							m_lStorageCurrent -= (m_lUserWinScore[wChairID]);						
						ScoreInfo[wChairID].lScore = m_lUserWinScore[wChairID];
						ScoreInfo[wChairID].cbType=m_lUserWinScore[wChairID]>0?SCORE_TYPE_WIN:SCORE_TYPE_LOSE;
						ScoreInfo[wChairID].lRevenue = m_lUserRevenue[wChairID];
						m_pITableFrame->WriteTableScore(ScoreInfo,CountArray(ScoreInfo));
						m_lUserWinScore[wChairID] = 0;
					}
					//清除下注
					for (int nAreaIndex = 1; nAreaIndex <= AREA_COUNT; nAreaIndex++)
					{
						m_lUserJettonScore[nAreaIndex][wChairID] = 0;
					}
				}

				return true;
			}

			//状态判断
			if (m_pITableFrame->GetGameStatus()!=GAME_SCENE_GAME_END)
			{
				//提示消息
				TCHAR szTipMsg[128];
				_sntprintf(szTipMsg,CountArray(szTipMsg),TEXT("由于庄家[ %s ]强退，游戏提前结束！"),pIServerUserItem->GetNickName());

				//发送消息
				SendGameMessage(INVALID_CHAIR,szTipMsg);	

				//设置状态
				m_pITableFrame->SetGameStatus(GAME_SCENE_GAME_END);

				//设置时间
				m_dwJettonTime=(DWORD)time(NULL);
				m_pITableFrame->KillGameTimer(IDI_PLACE_JETTON);
				m_pITableFrame->SetGameTimer(IDI_GAME_END,m_nGameEndTime*1000,1,0L);

				//游戏作弊
				if( m_pServerContro != NULL && m_pServerContro->NeedControl() )
				{
					//获取控制目标牌型
					m_pServerContro->GetSuitResult(m_cbTableCardArray, m_cbTableCard, m_lAllJettonScore);
					m_pServerContro->CompleteControl();
				}

				//计算分数
				CalculateScore();

				//结束消息
				CMD_S_GameEnd GameEnd;
				ZeroMemory(&GameEnd,sizeof(GameEnd));

				//占位玩家成绩
				for (WORD i=0; i<MAX_OCCUPY_SEAT_COUNT; i++)
				{
					if (m_wOccupySeatChairIDArray[i] == INVALID_CHAIR)
					{
						continue;
					}
					IServerUserItem *pIOccupySeatServerUserItem = m_pITableFrame->GetTableUserItem(m_wOccupySeatChairIDArray[i]);
					if (!pIOccupySeatServerUserItem)
					{
						continue;
					}

					GameEnd.lOccupySeatUserWinScore[i] = m_lUserWinScore[m_wOccupySeatChairIDArray[i]];
				}

				//庄家信息
				GameEnd.nBankerTime = m_wBankerTime;
				GameEnd.lBankerTotallScore=m_lBankerWinScore;
				GameEnd.lBankerScore=m_lUserWinScore[m_wCurrentBanker];

				//扑克信息
				CopyMemory(GameEnd.cbTableCardArray,m_cbTableCardArray,sizeof(m_cbTableCardArray));
				GameEnd.cbLeftCardCount=m_cbLeftCardCount;

				//发送积分
				GameEnd.cbTimeLeave=m_nGameEndTime;	
				for ( WORD wUserIndex = 0; wUserIndex < GAME_PLAYER; ++wUserIndex )
				{
					IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(wUserIndex);
					if ( pIServerUserItem == NULL ) continue;

					//设置成绩
					GameEnd.lUserScore=m_lUserWinScore[wUserIndex];

					//返还积分
					GameEnd.lUserReturnScore=m_lUserReturnScore[wUserIndex];

					//设置税收
					if (m_lUserRevenue[wUserIndex]>0) GameEnd.lRevenue=m_lUserRevenue[wUserIndex];
					else if (m_wCurrentBanker!=INVALID_CHAIR) GameEnd.lRevenue=m_lUserRevenue[m_wCurrentBanker];
					else GameEnd.lRevenue=0;

					//发送消息					
					m_pITableFrame->SendTableData(wUserIndex,SUB_S_GAME_END,&GameEnd,sizeof(GameEnd));
					m_pITableFrame->SendLookonData(wUserIndex,SUB_S_GAME_END,&GameEnd,sizeof(GameEnd));
				}
			}

			//扣除分数
			tagScoreInfo ScoreInfo[GAME_PLAYER];
			ZeroMemory(ScoreInfo,sizeof(ScoreInfo));
			ScoreInfo[m_wCurrentBanker].lScore =m_lUserWinScore[m_wCurrentBanker];
			ScoreInfo[m_wCurrentBanker].lRevenue = m_lUserRevenue[m_wCurrentBanker];
			ScoreInfo[m_wCurrentBanker].cbType = (m_lUserWinScore[m_wCurrentBanker]>0?SCORE_TYPE_WIN:SCORE_TYPE_LOSE);
			m_pITableFrame->WriteTableScore(ScoreInfo,CountArray(ScoreInfo));

			//库存金币
			IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(m_wCurrentBanker);
			if (pIServerUserItem != NULL && !pIServerUserItem->IsAndroidUser())
				m_lStorageCurrent -= (m_lUserWinScore[m_wCurrentBanker]);

			m_lUserWinScore[m_wCurrentBanker] = 0;

			//切换庄家
			ChangeBanker(true);

			return true;
		}
	}
	ASSERT(FALSE);
	return false;
}

//发送场景
bool CTableFrameSink::OnEventSendGameScene(WORD wChairID, IServerUserItem * pIServerUserItem, BYTE cbGameStatus, bool bSendSecret)
{
	switch (cbGameStatus)
	{
	case GAME_SCENE_FREE:			//空闲状态
		{
			//发送记录
			SendGameRecord(pIServerUserItem);

			//构造数据
			CMD_S_StatusFree StatusFree;
			ZeroMemory(&StatusFree,sizeof(StatusFree));			

			//控制信息
			StatusFree.lApplyBankerCondition = m_lApplyBankerCondition;
			StatusFree.lAreaLimitScore = m_lAreaLimitScore;
			StatusFree.bGenreEducate = m_pGameServiceOption->wServerType&GAME_GENRE_EDUCATE;
			//庄家信息
			StatusFree.bEnableSysBanker=m_bEnableSysBanker;
			StatusFree.wBankerUser=m_wCurrentBanker;	
			StatusFree.cbBankerTime=m_wBankerTime;
			StatusFree.lBankerWinScore=m_lBankerWinScore;
			//获取庄家
			IServerUserItem* pIBankerServerUserItem = NULL;
			if ( m_wCurrentBanker == INVALID_CHAIR )
			{
				m_lBankerScore = 1000000000;
			}
			else
			{
				IServerUserItem* pIBankerServerUserItem = m_pITableFrame->GetTableUserItem(m_wCurrentBanker);
				if ( pIBankerServerUserItem != NULL )
				{
					m_lBankerScore = pIBankerServerUserItem->GetUserScore();
				}
			}

			StatusFree.lBankerScore = m_lBankerScore;
			StatusFree.nEndGameMul = m_nEndGameMul;

			//玩家信息
			if (pIServerUserItem->GetUserStatus()!=US_LOOKON)
			{
				StatusFree.lUserMaxScore=min(pIServerUserItem->GetUserScore(),m_lUserLimitScore*10); 
			}

			//房间名称
			CopyMemory(StatusFree.szGameRoomName, m_pGameServiceOption->szServerName, sizeof(StatusFree.szGameRoomName));
			
			//超级抢庄
			CopyMemory(&(StatusFree.superbankerConfig), &m_superbankerConfig, sizeof(m_superbankerConfig));
			StatusFree.wCurSuperRobBankerUser = m_wCurSuperRobBankerUser;
			StatusFree.typeCurrentBanker = m_typeCurrentBanker;

			//占位
			CopyMemory(&(StatusFree.occupyseatConfig), &m_occupyseatConfig, sizeof(m_occupyseatConfig));
			CopyMemory(StatusFree.wOccupySeatChairIDArray, m_wOccupySeatChairIDArray, sizeof(m_wOccupySeatChairIDArray));

			//全局信息
			DWORD dwPassTime=(DWORD)time(NULL)-m_dwJettonTime;
			StatusFree.cbTimeLeave=(BYTE)(m_nFreeTime-__min(dwPassTime,(DWORD)m_nFreeTime));
			
			//机器人配置
			if(pIServerUserItem->IsAndroidUser())
			{
				tagCustomConfig *pCustomConfig = (tagCustomConfig *)m_pGameServiceOption->cbCustomRule;
				ASSERT(pCustomConfig);

				CopyMemory(&StatusFree.CustomAndroid, &pCustomConfig->CustomAndroid, sizeof(tagCustomAndroid));
			}

			//发送场景
			bool bSuccess = m_pITableFrame->SendGameScene(pIServerUserItem,&StatusFree,sizeof(StatusFree));

			//限制提示
			TCHAR szTipMsg[128];
			_sntprintf(szTipMsg,CountArray(szTipMsg),TEXT("本房间上庄条件为：%I64d,区域限制为：%I64d,玩家限制为：%I64d"),m_lApplyBankerCondition,
				m_lAreaLimitScore,m_lUserLimitScore);

			m_pITableFrame->SendGameMessage(pIServerUserItem,szTipMsg,SMT_CHAT);

			//发送申请者
			SendApplyUser(pIServerUserItem);
			
			//更新库存信息
			if(CUserRight::IsGameCheatUser(pIServerUserItem->GetUserRight()))
			{
				CMD_S_UpdateStorage updateStorage;
				ZeroMemory(&updateStorage, sizeof(updateStorage));
				
				updateStorage.cbReqType = RQ_REFRESH_STORAGE;
				updateStorage.lStorageStart = m_lStorageStart;
				updateStorage.lStorageDeduct = m_nStorageDeduct;
				updateStorage.lStorageCurrent = m_lStorageCurrent;
				updateStorage.lStorageMax1 = m_lStorageMax1;
				updateStorage.lStorageMul1 = m_lStorageMul1;
				updateStorage.lStorageMax2 = m_lStorageMax2;
				updateStorage.lStorageMul2 = m_lStorageMul2;
			
				m_pITableFrame->SendUserItemData(pIServerUserItem,SUB_S_UPDATE_STORAGE,&updateStorage,sizeof(updateStorage));
			}

			return bSuccess;
		}
	case GAME_SCENE_PLACE_JETTON:		//游戏状态
	case GAME_SCENE_GAME_END:			//结束状态
		{
			//发送记录
			SendGameRecord(pIServerUserItem);		

			//构造数据
			CMD_S_StatusPlay StatusPlay={0};

			//全局下注
			CopyMemory(StatusPlay.lAllJettonScore,m_lAllJettonScore,sizeof(StatusPlay.lAllJettonScore));

			//玩家下注
			if (pIServerUserItem->GetUserStatus()!=US_LOOKON)
			{
				for (int nAreaIndex=1; nAreaIndex<=AREA_COUNT; ++nAreaIndex)
					StatusPlay.lUserJettonScore[nAreaIndex] = m_lUserJettonScore[nAreaIndex][wChairID];

				//最大下注
				StatusPlay.lUserMaxScore=min(pIServerUserItem->GetUserScore(),m_lUserLimitScore*10);
			}

			//控制信息
			StatusPlay.lApplyBankerCondition=m_lApplyBankerCondition;		
			StatusPlay.lAreaLimitScore=m_lAreaLimitScore;		
			StatusPlay.bGenreEducate = m_pGameServiceOption->wServerType&GAME_GENRE_EDUCATE;
			//庄家信息
			StatusPlay.bEnableSysBanker=m_bEnableSysBanker;
			StatusPlay.wBankerUser=m_wCurrentBanker;			
			StatusPlay.cbBankerTime=m_wBankerTime;
			StatusPlay.lBankerWinScore=m_lBankerWinScore;	
			//获取庄家
			IServerUserItem* pIBankerServerUserItem = NULL;
			if ( m_wCurrentBanker == INVALID_CHAIR )
			{
				m_lBankerScore = 1000000000;
			}
			else
			{
				IServerUserItem* pIBankerServerUserItem = m_pITableFrame->GetTableUserItem(m_wCurrentBanker);
				if ( pIBankerServerUserItem != NULL )
				{
					m_lBankerScore = pIBankerServerUserItem->GetUserScore();
				}
			}

			StatusPlay.lBankerScore = m_lBankerScore;
			StatusPlay.nEndGameMul = m_nEndGameMul;

			//全局信息
			DWORD dwPassTime=(DWORD)time(NULL)-m_dwJettonTime;
			int	nTotalTime = (cbGameStatus==GAME_SCENE_PLACE_JETTON?m_nPlaceJettonTime:m_nGameEndTime);
			StatusPlay.cbTimeLeave=(BYTE)(nTotalTime-__min(dwPassTime,(DWORD)nTotalTime));
			StatusPlay.cbGameStatus=m_pITableFrame->GetGameStatus();			

			//房间名称
			CopyMemory(StatusPlay.szGameRoomName, m_pGameServiceOption->szServerName, sizeof(StatusPlay.szGameRoomName));
			
			//超级抢庄
			CopyMemory(&(StatusPlay.superbankerConfig), &m_superbankerConfig, sizeof(m_superbankerConfig));
			StatusPlay.wCurSuperRobBankerUser = m_wCurSuperRobBankerUser;
			StatusPlay.typeCurrentBanker = m_typeCurrentBanker;

			//占位
			CopyMemory(&(StatusPlay.occupyseatConfig), &m_occupyseatConfig, sizeof(m_occupyseatConfig));
			CopyMemory(StatusPlay.wOccupySeatChairIDArray, m_wOccupySeatChairIDArray, sizeof(m_wOccupySeatChairIDArray));

			//占位玩家成绩
			for (WORD i=0; i<MAX_OCCUPY_SEAT_COUNT; i++)
			{
				if (m_wOccupySeatChairIDArray[i] == INVALID_CHAIR)
				{
					continue;
				}
				IServerUserItem *pIOccupySeatServerUserItem = m_pITableFrame->GetTableUserItem(m_wOccupySeatChairIDArray[i]);
				if (!pIOccupySeatServerUserItem)
				{
					continue;
				}

				StatusPlay.lOccupySeatUserWinScore[i] = m_lUserWinScore[m_wOccupySeatChairIDArray[i]];
			}

			//结束判断
			if (cbGameStatus==GAME_SCENE_GAME_END)
			{
				//设置成绩
				StatusPlay.lEndUserScore=m_lUserWinScore[wChairID];

				//返还积分
				StatusPlay.lEndUserReturnScore=m_lUserReturnScore[wChairID];

				//设置税收
				if (m_lUserRevenue[wChairID]>0) StatusPlay.lEndRevenue=m_lUserRevenue[wChairID];
				else if (m_wCurrentBanker!=INVALID_CHAIR) StatusPlay.lEndRevenue=m_lUserRevenue[m_wCurrentBanker];
				else StatusPlay.lEndRevenue=0;

				//庄家成绩
				StatusPlay.lEndBankerScore=m_lBankerCurGameScore;

				//扑克信息
				CopyMemory(StatusPlay.cbTableCardArray,m_cbTableCardArray,sizeof(m_cbTableCardArray));
			}
			
			//机器人配置
			if(pIServerUserItem->IsAndroidUser())
			{
				tagCustomConfig *pCustomConfig = (tagCustomConfig *)m_pGameServiceOption->cbCustomRule;
				ASSERT(pCustomConfig);

				CopyMemory(&StatusPlay.CustomAndroid, &pCustomConfig->CustomAndroid, sizeof(tagCustomAndroid));
			}

			//发送场景
			bool bSuccess = m_pITableFrame->SendGameScene(pIServerUserItem,&StatusPlay,sizeof(StatusPlay));

			//限制提示
			TCHAR szTipMsg[128];
			_sntprintf(szTipMsg,CountArray(szTipMsg),TEXT("本房间上庄条件为：%I64d,区域限制为：%I64d,玩家限制为：%I64d"),m_lApplyBankerCondition,
				m_lAreaLimitScore,m_lUserLimitScore);

			m_pITableFrame->SendGameMessage(pIServerUserItem,szTipMsg,SMT_CHAT);

			//发送申请者
			SendApplyUser( pIServerUserItem );
			
			//更新库存信息
			if(CUserRight::IsGameCheatUser(pIServerUserItem->GetUserRight()))
			{
				CMD_S_UpdateStorage updateStorage;
				ZeroMemory(&updateStorage, sizeof(updateStorage));
				
				updateStorage.cbReqType = RQ_REFRESH_STORAGE;
				updateStorage.lStorageStart = m_lStorageStart;
				updateStorage.lStorageDeduct = m_nStorageDeduct;
				updateStorage.lStorageCurrent = m_lStorageCurrent;
				updateStorage.lStorageMax1 = m_lStorageMax1;
				updateStorage.lStorageMul1 = m_lStorageMul1;
				updateStorage.lStorageMax2 = m_lStorageMax2;
				updateStorage.lStorageMul2 = m_lStorageMul2;
			
				m_pITableFrame->SendUserItemData(pIServerUserItem,SUB_S_UPDATE_STORAGE,&updateStorage,sizeof(updateStorage));
			}
			
			//发送玩家下注信息
			if(CUserRight::IsGameCheatUser(pIServerUserItem->GetUserRight()))
			{
				SendUserBetInfo(pIServerUserItem);
			}

			return bSuccess;
		}
	}

	return false;
}

//定时器事件
bool CTableFrameSink::OnTimerMessage(DWORD wTimerID, WPARAM wBindParam)
{
	switch (wTimerID)
	{
	case IDI_FREE:		//空闲时间
		{
			//开始游戏
			m_pITableFrame->StartGame();

			//设置时间
			m_dwJettonTime=(DWORD)time(NULL);
			m_pITableFrame->SetGameTimer(IDI_PLACE_JETTON,m_nPlaceJettonTime*1000,1,0L);

			//派发扑克
			DispatchTableCard();

			//设置状态
			m_pITableFrame->SetGameStatus(GAME_SCENE_PLACE_JETTON);

			return true;
		}
	case IDI_PLACE_JETTON:		//下注时间
		{
			//状态判断(防止强退重复设置)
			if (m_pITableFrame->GetGameStatus()!=GAME_SCENE_GAME_END)
			{
				//设置状态
				m_pITableFrame->SetGameStatus(GAME_SCENE_GAME_END);			

				//结束游戏
				OnEventGameConclude(INVALID_CHAIR,NULL,GER_NORMAL);

				//设置时间
				m_dwJettonTime=(DWORD)time(NULL);
				m_pITableFrame->SetGameTimer(IDI_GAME_END,m_nGameEndTime*1000,1,0L);		
			}

			return true;
		}
	case IDI_GAME_END:			//结束游戏
		{
			//写入积分
			tagScoreInfo ScoreInfo[GAME_PLAYER];
			ZeroMemory(ScoreInfo,sizeof(ScoreInfo));
			for ( WORD wUserChairID = 0; wUserChairID < GAME_PLAYER; ++wUserChairID )
			{
				IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(wUserChairID);				
				if ( pIServerUserItem == NULL||(QueryBuckleServiceCharge(wUserChairID)==false)) continue;				
				
				//胜利类型
				BYTE ScoreKind=(m_lUserWinScore[wUserChairID]>0L)?SCORE_TYPE_WIN:SCORE_TYPE_LOSE;
				ScoreInfo[wUserChairID].lScore = m_lUserWinScore[wUserChairID];
				ScoreInfo[wUserChairID].lRevenue = m_lUserRevenue[wUserChairID];
				ScoreInfo[wUserChairID].cbType = ScoreKind;

				//库存金币
				if (!pIServerUserItem->IsAndroidUser())
				{
					m_lStorageCurrent -= (m_lUserWinScore[wUserChairID]);					
				}
			}
			
			//写入积分
			m_pITableFrame->WriteTableScore(ScoreInfo,CountArray(ScoreInfo));

			LONGLONG lTempDeduct=0;
			lTempDeduct=m_nStorageDeduct;
			bool bDeduct=NeedDeductStorage();
			lTempDeduct=bDeduct?lTempDeduct:0;
			//库存衰减
			if (m_lStorageCurrent > 0)
				m_lStorageCurrent = m_lStorageCurrent - m_lStorageCurrent*lTempDeduct/1000;

			//设置时间
			m_dwJettonTime=(DWORD)time(NULL);
			m_pITableFrame->SetGameTimer(IDI_FREE,m_nFreeTime*1000,1,0L);

			//发送消息
			CMD_S_GameFree GameFree;
			ZeroMemory(&GameFree,sizeof(GameFree));
			GameFree.cbTimeLeave=m_nFreeTime;
			GameFree.lStorageStart=m_lStorageCurrent;
			GameFree.nListUserCount=m_ApplyUserArray.GetCount()-1;
			m_pITableFrame->SendTableData(INVALID_CHAIR,SUB_S_GAME_FREE,&GameFree,sizeof(GameFree));
			m_pITableFrame->SendLookonData(INVALID_CHAIR,SUB_S_GAME_FREE,&GameFree,sizeof(GameFree));
			
			//结束游戏
			m_pITableFrame->ConcludeGame(GAME_SCENE_FREE);
			
			//切换庄家
			ChangeBanker(false);

			//更新库存信息
			for ( WORD wUserID = 0; wUserID < GAME_PLAYER; ++wUserID )
			{
				IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(wUserID);
				if ( pIServerUserItem == NULL ) continue;
				if( !CUserRight::IsGameCheatUser( pIServerUserItem->GetUserRight() ) ) continue;

				CMD_S_UpdateStorage updateStorage;
				ZeroMemory(&updateStorage, sizeof(updateStorage));
				
				updateStorage.cbReqType = RQ_REFRESH_STORAGE;
				updateStorage.lStorageStart = m_lStorageStart;
				updateStorage.lStorageDeduct = m_nStorageDeduct;
				updateStorage.lStorageCurrent = m_lStorageCurrent;
				updateStorage.lStorageMax1 = m_lStorageMax1;
				updateStorage.lStorageMul1 = m_lStorageMul1;
				updateStorage.lStorageMax2 = m_lStorageMax2;
				updateStorage.lStorageMul2 = m_lStorageMul2;
			
				m_pITableFrame->SendUserItemData(pIServerUserItem,SUB_S_UPDATE_STORAGE,&updateStorage,sizeof(updateStorage));
			}

			return true;
		}
	}

	return false;
}

//游戏消息
bool CTableFrameSink::OnGameMessage(WORD wSubCmdID, VOID * pData, WORD wDataSize, IServerUserItem * pIServerUserItem)
{
	switch (wSubCmdID)
	{
	case SUB_C_PLACE_JETTON:		//用户加注
		{
			//效验数据
			ASSERT(wDataSize==sizeof(CMD_C_PlaceJetton));
			if (wDataSize!=sizeof(CMD_C_PlaceJetton)) return false;

			//用户效验
			tagUserInfo * pUserData=pIServerUserItem->GetUserInfo();
			if (pUserData->cbUserStatus!=US_PLAYING) return true;

			//消息处理
			CMD_C_PlaceJetton * pPlaceJetton=(CMD_C_PlaceJetton *)pData;
			return OnUserPlaceJetton(pUserData->wChairID,pPlaceJetton->cbJettonArea,pPlaceJetton->lJettonScore);
		}
	case SUB_C_APPLY_BANKER:		//申请做庄
		{
			//用户效验
			tagUserInfo * pUserData=pIServerUserItem->GetUserInfo();
			if (pUserData->cbUserStatus==US_LOOKON) return true;

			return OnUserApplyBanker(pIServerUserItem);	
		}
	case SUB_C_CANCEL_BANKER:		//取消做庄
		{
			//用户效验
			tagUserInfo * pUserData=pIServerUserItem->GetUserInfo();
			if (pUserData->cbUserStatus==US_LOOKON) return true;

			return OnUserCancelBanker(pIServerUserItem);	
		}
	case SUB_C_CONTINUE_CARD:		//继续发牌
		{
			//用户效验
			tagUserInfo * pUserData=pIServerUserItem->GetUserInfo();
			if (pUserData->cbUserStatus==US_LOOKON) return true;
			if (pUserData->wChairID!=m_wCurrentBanker) return true;
			if (m_cbLeftCardCount < 8) return true;

			//设置变量
			m_bContiueCard=true;

			//发送消息
			SendGameMessage(pUserData->wChairID,TEXT("设置成功，下一局将继续发牌！"));

			return true;
		}
	case SUB_C_AMDIN_COMMAND:
		{
			ASSERT(wDataSize==sizeof(CMD_C_AdminReq));
			if(wDataSize!=sizeof(CMD_C_AdminReq)) return false;

			//权限判断
			if(CUserRight::IsGameCheatUser(pIServerUserItem->GetUserRight())==false)
				return false;

			if ( m_pServerContro == NULL)
				return false;
			CopyMemory(m_szControlName,pIServerUserItem->GetNickName(),sizeof(m_szControlName));

			return m_pServerContro->ServerControl(wSubCmdID, pData, wDataSize, pIServerUserItem, m_pITableFrame, m_pGameServiceOption);
		}
	case SUB_C_UPDATE_STORAGE:		//更新库存
		{
			ASSERT(wDataSize==sizeof(CMD_C_UpdateStorage));
			if(wDataSize!=sizeof(CMD_C_UpdateStorage)) return false;

			//权限判断
			if(CUserRight::IsGameCheatUser(pIServerUserItem->GetUserRight())==false)
				return false;

			//消息处理
			CMD_C_UpdateStorage * pUpdateStorage=(CMD_C_UpdateStorage *)pData;
			if (pUpdateStorage->cbReqType==RQ_SET_STORAGE)
			{
				m_nStorageDeduct = pUpdateStorage->lStorageDeduct;
				m_lStorageCurrent = pUpdateStorage->lStorageCurrent;
				m_lStorageMax1 = pUpdateStorage->lStorageMax1;
				m_lStorageMul1 = pUpdateStorage->lStorageMul1;
				m_lStorageMax2 = pUpdateStorage->lStorageMax2;
				m_lStorageMul2 = pUpdateStorage->lStorageMul2;

				//记录信息
				CString strControlInfo, str;
				str.Format(TEXT("%s"), TEXT("修改库存设置！"));
				CTime Time(CTime::GetCurrentTime());
				strControlInfo.Format(TEXT("房间: %s | 桌号: %u | 时间: %d-%d-%d %d:%d:%d\n控制人账号: %s | 控制人ID: %u\n%s\r\n"),
					m_pGameServiceOption->szServerName, m_pITableFrame->GetTableID()+1, Time.GetYear(), Time.GetMonth(), Time.GetDay(),
					Time.GetHour(), Time.GetMinute(), Time.GetSecond(), pIServerUserItem->GetNickName(), pIServerUserItem->GetGameID(), str);
				
				CString strFileName;
				strFileName.Format(TEXT("百人牛牛[%s]控制信息.log"), m_pGameServiceOption->szServerName);

				WriteInfo(strFileName, strControlInfo);
			}
			
			for(WORD wUserID = 0; wUserID < GAME_PLAYER; wUserID++)
			{
				IServerUserItem *pIServerUserItemSend = m_pITableFrame->GetTableUserItem(wUserID);
				if ( pIServerUserItemSend == NULL ) continue;
				if( !CUserRight::IsGameCheatUser( pIServerUserItemSend->GetUserRight() ) ) continue;

				if(RQ_REFRESH_STORAGE == pUpdateStorage->cbReqType && pIServerUserItem->GetChairID() != wUserID) continue;

				CMD_S_UpdateStorage updateStorage;
				ZeroMemory(&updateStorage, sizeof(updateStorage));
				
				if(RQ_SET_STORAGE == pUpdateStorage->cbReqType && pIServerUserItem->GetChairID() == wUserID)
				{
					updateStorage.cbReqType = RQ_SET_STORAGE;
				}
				else
				{
					updateStorage.cbReqType = RQ_REFRESH_STORAGE;
				}

				updateStorage.lStorageStart = m_lStorageStart;
				updateStorage.lStorageDeduct = m_nStorageDeduct;
				updateStorage.lStorageCurrent = m_lStorageCurrent;
				updateStorage.lStorageMax1 = m_lStorageMax1;
				updateStorage.lStorageMul1 = m_lStorageMul1;
				updateStorage.lStorageMax2 = m_lStorageMax2;
				updateStorage.lStorageMul2 = m_lStorageMul2;

				m_pITableFrame->SendUserItemData(pIServerUserItem,SUB_S_UPDATE_STORAGE,&updateStorage,sizeof(updateStorage));
			}

			return true;
		}
	case SUB_C_SUPERROB_BANKER:
		{
			//用户效验
			if ( pIServerUserItem->GetUserStatus() == US_LOOKON ) 
			{
				return true;
			}

			//校验条件
			if (m_superbankerConfig.superbankerType == SUPERBANKER_VIPTYPE)
			{
				ASSERT (pIServerUserItem->GetMemberOrder() >= GetMemberOrderIndex(m_superbankerConfig.enVipIndex));

				if (!(pIServerUserItem->GetMemberOrder() >= GetMemberOrderIndex(m_superbankerConfig.enVipIndex)))
				{
					return true;
				}
			}
			else if (m_superbankerConfig.superbankerType == SUPERBANKER_CONSUMETYPE)
			{
				ASSERT (pIServerUserItem->GetUserScore() > m_superbankerConfig.lSuperBankerConsume);

				if (!(pIServerUserItem->GetUserScore() > m_superbankerConfig.lSuperBankerConsume))
				{
					return true;
				}
			}

			//已有超级抢庄玩家
			if (m_wCurSuperRobBankerUser != INVALID_CHAIR)
			{
				CMD_S_SuperRobBanker SuperRobBanker;
				ZeroMemory(&SuperRobBanker, sizeof(SuperRobBanker));
				SuperRobBanker.bSucceed = false;
				SuperRobBanker.wCurSuperRobBankerUser = m_wCurSuperRobBankerUser;
				SuperRobBanker.wApplySuperRobUser = pIServerUserItem->GetChairID();

				//发送消息
				m_pITableFrame->SendTableData(INVALID_CHAIR, SUB_S_SUPERROB_BANKER, &SuperRobBanker, sizeof(SuperRobBanker));
				m_pITableFrame->SendLookonData(INVALID_CHAIR, SUB_S_SUPERROB_BANKER, &SuperRobBanker, sizeof(SuperRobBanker));
			}
			else
			{
				CMD_S_SuperRobBanker SuperRobBanker;
				ZeroMemory(&SuperRobBanker, sizeof(SuperRobBanker));
				SuperRobBanker.bSucceed = true;
				SuperRobBanker.wCurSuperRobBankerUser = pIServerUserItem->GetChairID();
				SuperRobBanker.wApplySuperRobUser = pIServerUserItem->GetChairID();
				m_wCurSuperRobBankerUser = pIServerUserItem->GetChairID();

				//超级抢庄消耗类型
				if (m_superbankerConfig.superbankerType == SUPERBANKER_CONSUMETYPE)
				{
					tagScoreInfo ScoreInfo;
					ZeroMemory(&ScoreInfo, sizeof(ScoreInfo));
					ScoreInfo.cbType = SCORE_TYPE_SERVICE;
					ScoreInfo.lRevenue = 0;
					ScoreInfo.lScore = -m_superbankerConfig.lSuperBankerConsume;
					m_pITableFrame->WriteUserScore(pIServerUserItem->GetChairID(), ScoreInfo);
				}

				m_ApplyUserArray.Add(INVALID_CHAIR);
				//改变上庄列表顺序
				for (WORD i=m_ApplyUserArray.GetCount()-1; i>0; i--)
				{
					m_ApplyUserArray[i] = m_ApplyUserArray[i-1];
				}

				m_ApplyUserArray[0] = pIServerUserItem->GetChairID();


				//发送消息
				m_pITableFrame->SendTableData(INVALID_CHAIR, SUB_S_SUPERROB_BANKER, &SuperRobBanker, sizeof(SuperRobBanker));
				m_pITableFrame->SendLookonData(INVALID_CHAIR, SUB_S_SUPERROB_BANKER, &SuperRobBanker, sizeof(SuperRobBanker));
			}

			return true;
		}
	case SUB_C_OCCUPYSEAT:
		{
			//效验数据
			ASSERT (wDataSize == sizeof(CMD_C_OccupySeat));
			if (wDataSize != sizeof(CMD_C_OccupySeat))
			{
				return true;
			}

			//消息处理
			CMD_C_OccupySeat *pOccupySeat = (CMD_C_OccupySeat *)pData;
			return OnUserOccupySeat(pOccupySeat->wOccupySeatChairID, pOccupySeat->cbOccupySeatIndex);
		}
	case SUB_C_QUIT_OCCUPYSEAT:
		{	
			bool binvalid = false;
			//校验数据
			for (WORD i=0; i<MAX_OCCUPY_SEAT_COUNT; i++)
			{
				if (m_wOccupySeatChairIDArray[i] == pIServerUserItem->GetChairID())
				{
					binvalid = true;

					//重置无效
					m_wOccupySeatChairIDArray[i] = INVALID_CHAIR;

					break;
				}
			}

			ASSERT (binvalid == true);

			CMD_S_UpdateOccupySeat UpdateOccupySeat;
			ZeroMemory(&UpdateOccupySeat, sizeof(UpdateOccupySeat));
			CopyMemory(UpdateOccupySeat.wOccupySeatChairIDArray, m_wOccupySeatChairIDArray, sizeof(m_wOccupySeatChairIDArray));
			UpdateOccupySeat.wQuitOccupySeatChairID = pIServerUserItem->GetChairID();

			//发送数据
			m_pITableFrame->SendTableData(INVALID_CHAIR, SUB_S_UPDATE_OCCUPYSEAT, &UpdateOccupySeat, sizeof(UpdateOccupySeat));
			m_pITableFrame->SendLookonData(INVALID_CHAIR, SUB_S_UPDATE_OCCUPYSEAT, &UpdateOccupySeat, sizeof(UpdateOccupySeat));

			return true;
		}
	}

	return false;
}

//框架消息
bool CTableFrameSink::OnFrameMessage(WORD wSubCmdID, VOID * pData, WORD wDataSize, IServerUserItem * pIServerUserItem)
{
	return false;
}

bool CTableFrameSink::OnActionUserOffLine(WORD wChairID,IServerUserItem * pIServerUserItem)
{
	//离线庄家
	if (wChairID == m_wCurrentBanker && pIServerUserItem->IsAndroidUser() == false)
	{
		m_wOfflineBanker = wChairID;
	}

	if (wChairID == m_wCurSuperRobBankerUser)
	{
		m_wCurSuperRobBankerUser = INVALID_CHAIR;

		CMD_S_CurSuperRobLeave CurSuperRobLeave;
		ZeroMemory(&CurSuperRobLeave,sizeof(CurSuperRobLeave));

		//设置变量
		CurSuperRobLeave.wCurSuperRobBankerUser = m_wCurSuperRobBankerUser;

		//发送消息
		m_pITableFrame->SendTableData(INVALID_CHAIR, SUB_S_CURSUPERROB_LEAVE, &CurSuperRobLeave, sizeof(CurSuperRobLeave));
		m_pITableFrame->SendLookonData(INVALID_CHAIR, SUB_S_CURSUPERROB_LEAVE, &CurSuperRobLeave, sizeof(CurSuperRobLeave));
	}

	if(pIServerUserItem == NULL) return false;
	//切换庄家
	if (wChairID==m_wCurrentBanker) ChangeBanker(true);

	//取消申请
	for (WORD i=0; i<m_ApplyUserArray.GetCount(); ++i)
	{
		if (wChairID!=m_ApplyUserArray[i]) continue;

		//删除玩家
		m_ApplyUserArray.RemoveAt(i);

		//构造变量
		CMD_S_CancelBanker CancelBanker;
		ZeroMemory(&CancelBanker,sizeof(CancelBanker));

		//设置变量
		CancelBanker.wCancelUser=wChairID;

		//发送消息
		m_pITableFrame->SendTableData(INVALID_CHAIR, SUB_S_CANCEL_BANKER, &CancelBanker, sizeof(CancelBanker));
		m_pITableFrame->SendLookonData(INVALID_CHAIR, SUB_S_CANCEL_BANKER, &CancelBanker, sizeof(CancelBanker));

		break;
	}
	CString strbanker;
	if (m_wCurrentBanker == INVALID_CHAIR)
	{
		strbanker = TEXT("系统坐庄 ");
	}
	else
	{
		strbanker = m_pITableFrame->GetTableUserItem(m_wCurrentBanker)->IsAndroidUser() ? TEXT("机器人坐庄 ") : TEXT("真人坐庄 ");
	}

	CString strDebug;
	CTime Time(CTime::GetCurrentTime());
	strDebug.Format(TEXT("掉线玩家昵称[%s], %s| 时间: %d-%d-%d %d:%d:%d | 库存: %I64d \n"),  pIServerUserItem->GetNickName(), strbanker, Time.GetYear(), Time.GetMonth(), Time.GetDay(), Time.GetHour(), Time.GetMinute(), Time.GetSecond(), m_lStorageCurrent );

	CString strFileName;
	strFileName.Format(TEXT("百人牛牛[%s]掉线日志.log"), m_pGameServiceOption->szServerName);
	WriteInfo(strFileName, strDebug);

	return true;

}
//用户坐下
bool CTableFrameSink::OnActionUserSitDown(WORD wChairID,IServerUserItem * pIServerUserItem, bool bLookonUser)
{
	//起始分数
	m_lUserStartScore[wChairID] = pIServerUserItem->GetUserScore();

	//设置时间
	if ((bLookonUser==false)&&(m_dwJettonTime==0L))
	{
		m_dwJettonTime=(DWORD)time(NULL);
		m_pITableFrame->SetGameTimer(IDI_FREE,m_nFreeTime*1000,1,NULL);
		m_pITableFrame->SetGameStatus(GAME_SCENE_FREE);
	}
	
	//限制提示
	TCHAR szTipMsg[128];
	_sntprintf(szTipMsg,CountArray(szTipMsg),TEXT("本房间上庄金币必须大于：%I64d,区域限制为：%I64d,玩家限制为：%I64d"),m_lApplyBankerCondition,
		m_lAreaLimitScore,m_lUserLimitScore);
	m_pITableFrame->SendGameMessage(pIServerUserItem,szTipMsg,SMT_CHAT|SMT_EJECT);

	return true;
}

//用户起立
bool CTableFrameSink::OnActionUserStandUp(WORD wChairID,IServerUserItem * pIServerUserItem, bool bLookonUser)
{
	ASSERT(pIServerUserItem !=NULL);

	//起始分数
	m_lUserStartScore[wChairID] = 0;
	
	if (wChairID == m_wCurSuperRobBankerUser)
	{
		m_wCurSuperRobBankerUser = INVALID_CHAIR;

		CMD_S_CurSuperRobLeave CurSuperRobLeave;
		ZeroMemory(&CurSuperRobLeave,sizeof(CurSuperRobLeave));

		//设置变量
		CurSuperRobLeave.wCurSuperRobBankerUser = m_wCurSuperRobBankerUser;

		//发送消息
		m_pITableFrame->SendTableData(INVALID_CHAIR, SUB_S_CURSUPERROB_LEAVE, &CurSuperRobLeave, sizeof(CurSuperRobLeave));
		m_pITableFrame->SendLookonData(INVALID_CHAIR, SUB_S_CURSUPERROB_LEAVE, &CurSuperRobLeave, sizeof(CurSuperRobLeave));
	}

	bool bvalid = false;
	//校验数据
	for (WORD i=0; i<MAX_OCCUPY_SEAT_COUNT; i++)
	{
		if (m_wOccupySeatChairIDArray[i] == wChairID)
		{
			bvalid = true;

			//重置无效
			m_wOccupySeatChairIDArray[i] = INVALID_CHAIR;

			break;
		}
	}

	if (bvalid == true)
	{
		CMD_S_UpdateOccupySeat UpdateOccupySeat;
		ZeroMemory(&UpdateOccupySeat, sizeof(UpdateOccupySeat));
		CopyMemory(UpdateOccupySeat.wOccupySeatChairIDArray, m_wOccupySeatChairIDArray, sizeof(m_wOccupySeatChairIDArray));
		UpdateOccupySeat.wQuitOccupySeatChairID = wChairID;

		//发送数据
		m_pITableFrame->SendTableData(INVALID_CHAIR, SUB_S_UPDATE_OCCUPYSEAT, &UpdateOccupySeat, sizeof(UpdateOccupySeat));
		m_pITableFrame->SendLookonData(INVALID_CHAIR, SUB_S_UPDATE_OCCUPYSEAT, &UpdateOccupySeat, sizeof(UpdateOccupySeat));
	}

	//记录成绩
	if (bLookonUser==false)
	{
		//切换庄家
		if (wChairID==m_wCurrentBanker)
		{
			ChangeBanker(true);
			m_bContiueCard=false;
		}

		//取消申请
		for (WORD i=0; i<m_ApplyUserArray.GetCount(); ++i)
		{
			if (wChairID!=m_ApplyUserArray[i]) continue;

			//删除玩家
			m_ApplyUserArray.RemoveAt(i);

			//构造变量
			CMD_S_CancelBanker CancelBanker;
			ZeroMemory(&CancelBanker,sizeof(CancelBanker));

			//设置变量
			CancelBanker.wCancelUser=wChairID;

			//发送消息
			m_pITableFrame->SendTableData(INVALID_CHAIR, SUB_S_CANCEL_BANKER, &CancelBanker, sizeof(CancelBanker));
			m_pITableFrame->SendLookonData(INVALID_CHAIR, SUB_S_CANCEL_BANKER, &CancelBanker, sizeof(CancelBanker));

			break;
		}
	}

	return true;
}


//加注事件
bool CTableFrameSink::OnUserPlaceJetton(WORD wChairID, BYTE cbJettonArea, LONGLONG lJettonScore)
{
	//效验参数
	ASSERT((cbJettonArea<=ID_HUANG_MEN && cbJettonArea>=ID_TIAN_MEN)&&(lJettonScore>0L));
	if ((cbJettonArea>ID_HUANG_MEN)||(lJettonScore<=0L) || cbJettonArea<ID_TIAN_MEN)
	{
		return false;
	}
	////效验状态
	//ASSERT(m_pITableFrame->GetGameStatus()==GS_PLACE_JETTON);

	if (m_pITableFrame->GetGameStatus()!=GAME_SCENE_PLACE_JETTON)
	{
		CMD_S_PlaceJettonFail PlaceJettonFail;
		ZeroMemory(&PlaceJettonFail,sizeof(PlaceJettonFail));
		PlaceJettonFail.lJettonArea=cbJettonArea;
		PlaceJettonFail.lPlaceScore=lJettonScore;
		PlaceJettonFail.wPlaceUser=wChairID;

		//发送消息
		m_pITableFrame->SendTableData(wChairID,SUB_S_PLACE_JETTON_FAIL,&PlaceJettonFail,sizeof(PlaceJettonFail));
		return true;
	}
	//庄家判断
	if (m_wCurrentBanker==wChairID)
	{
		return true;
	}
	if (m_bEnableSysBanker==false && m_wCurrentBanker==INVALID_CHAIR) 
	{
		return true;
	}

	//庄家积分
	LONGLONG lBankerScore = 0;
	if(INVALID_CHAIR != m_wCurrentBanker)
	{
		IServerUserItem * pIServerUserItemBanker = m_pITableFrame->GetTableUserItem(m_wCurrentBanker);
		lBankerScore = pIServerUserItemBanker->GetUserScore();
	}

	//变量定义
	IServerUserItem * pIServerUserItem=m_pITableFrame->GetTableUserItem(wChairID);
	LONGLONG lJettonCount=0L;
	for (int nAreaIndex=1; nAreaIndex<=AREA_COUNT; ++nAreaIndex) lJettonCount += m_lUserJettonScore[nAreaIndex][wChairID];
	LONGLONG lAllJettonCount=0L;
	for (int nAreaIndex=1; nAreaIndex<=AREA_COUNT; ++nAreaIndex) lAllJettonCount += m_lAllJettonScore[nAreaIndex];

	//玩家积分
	LONGLONG lUserScore = pIServerUserItem->GetUserScore();

	//合法校验
	if (lUserScore < lJettonCount + lJettonScore) return true;
	if (m_lUserLimitScore < lJettonCount + lJettonScore) return true;

	//成功标识
	bool bPlaceJettonSuccess=true;

	//合法验证
	if (GetUserMaxJetton(wChairID, cbJettonArea) >= lJettonScore)
	{
		//机器人验证
		if(pIServerUserItem->IsAndroidUser())
		{
			//区域限制
			if (m_lRobotAreaScore[cbJettonArea] + lJettonScore > m_lRobotAreaLimit)
				return true;

			//数目限制
			bool bHaveChip = false;
			for (int i = 0; i <= AREA_COUNT; i++)
			{
				if (m_lUserJettonScore[i+1][wChairID] != 0)
					bHaveChip = true;
			}

			if (!bHaveChip)
			{
				if (m_nChipRobotCount+1 > m_nMaxChipRobot)
				{
					bPlaceJettonSuccess = false;
				}
				else
					m_nChipRobotCount++;
			}

			//统计分数
			if (bPlaceJettonSuccess)
				m_lRobotAreaScore[cbJettonArea] += lJettonScore;
		}

		if (bPlaceJettonSuccess)
		{
			//保存下注
			m_lAllJettonScore[cbJettonArea] += lJettonScore;
			m_lUserJettonScore[cbJettonArea][wChairID] += lJettonScore;			
		}
	}
	else
	{
		bPlaceJettonSuccess=false;
	}

	if (bPlaceJettonSuccess)
	{
		for(WORD i=0; i<GAME_PLAYER; i++)
		{
			IServerUserItem *pIServerUserItemSend = m_pITableFrame->GetTableUserItem(i);
			if ( pIServerUserItemSend == NULL ) continue;
			
			bool bIsGameCheatUser =  CUserRight::IsGameCheatUser(pIServerUserItemSend->GetUserRight());

			//变量定义
			CMD_S_PlaceJetton PlaceJetton;
			ZeroMemory(&PlaceJetton,sizeof(PlaceJetton));

			//构造变量
			PlaceJetton.wChairID=wChairID;
			PlaceJetton.cbJettonArea=cbJettonArea;
			PlaceJetton.lJettonScore=lJettonScore;
			
			PlaceJetton.bIsAndroid = m_pITableFrame->GetTableUserItem(wChairID)->IsAndroidUser();
			PlaceJetton.bAndroid = m_pITableFrame->GetTableUserItem(wChairID)->IsAndroidUser();


			//发送消息
			m_pITableFrame->SendTableData(i,SUB_S_PLACE_JETTON,&PlaceJetton,sizeof(PlaceJetton));
			m_pITableFrame->SendLookonData(i,SUB_S_PLACE_JETTON,&PlaceJetton,sizeof(PlaceJetton));
		}

		//发送玩家下注信息
		if(!pIServerUserItem->IsAndroidUser())
		{
			for(WORD i=0; i<GAME_PLAYER; i++)
			{
				IServerUserItem * pIServerUserItemSend = m_pITableFrame->GetTableUserItem(i);
				if(NULL == pIServerUserItemSend) continue;
				if(!CUserRight::IsGameCheatUser(pIServerUserItemSend->GetUserRight())) continue;

				SendUserBetInfo(pIServerUserItemSend);
			}
		}

		//下注总额超过庄家能支付上限时提前结束游戏
		if(INVALID_CHAIR != m_wCurrentBanker)
		{
			LONGLONG lBankerCanUseScore = lBankerScore*m_nEndGameMul/100;
			if(lBankerCanUseScore <= (lAllJettonCount + lJettonScore + 100)*10)
			{
				if (m_pITableFrame->GetGameStatus()!=GAME_SCENE_GAME_END)
				{
					for(WORD i=0; i<GAME_PLAYER; i++)
					{
						IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(i);
						if (pIServerUserItem == NULL) 
						{
							continue;
						}
						if (pIServerUserItem->IsAndroidUser())
						{
							continue;
						}
						//发送消息
						m_pITableFrame->SendTableData(i, SUB_S_ADVANCE_OPENCARD);
						m_pITableFrame->SendLookonData(i, SUB_S_ADVANCE_OPENCARD);
					}


					//设置状态
					m_pITableFrame->SetGameStatus(GAME_SCENE_GAME_END);			

					//结束游戏
					OnEventGameConclude(INVALID_CHAIR,NULL,GER_NORMAL);

					//设置时间
					m_dwJettonTime=(DWORD)time(NULL);
					m_pITableFrame->SetGameTimer(IDI_GAME_END,m_nGameEndTime*1000,1,0L);	
				}
			}
		}
	}
	else
	{
		CMD_S_PlaceJettonFail PlaceJettonFail;
		ZeroMemory(&PlaceJettonFail,sizeof(PlaceJettonFail));
		PlaceJettonFail.lJettonArea=cbJettonArea;
		PlaceJettonFail.lPlaceScore=lJettonScore;
		PlaceJettonFail.wPlaceUser=wChairID;

		//发送消息
		m_pITableFrame->SendTableData(wChairID,SUB_S_PLACE_JETTON_FAIL,&PlaceJettonFail,sizeof(PlaceJettonFail));
	}

	return true;
}

//发送扑克
bool CTableFrameSink::DispatchTableCard()
{
	//继续发牌
	int const static nDispatchCardCount=25;
	bool bContiueCard=m_bContiueCard;
	if (m_cbLeftCardCount<nDispatchCardCount) bContiueCard=false;

	//继续发牌
	if (true==bContiueCard)
	{
		//设置扑克
		CopyMemory(&m_cbTableCardArray[0][0],m_cbTableCard,sizeof(m_cbTableCardArray));
		m_cbLeftCardCount -= nDispatchCardCount;
		if (m_cbLeftCardCount>0) CopyMemory(m_cbTableCard, m_cbTableCard + nDispatchCardCount, sizeof(BYTE)*m_cbLeftCardCount);
	}
	else
	{
		//重新洗牌
		BYTE bcCard[CARD_COUNT];
		m_GameLogic.RandCardList(bcCard,54);
		m_bcFirstPostCard = bcCard[0];
		m_GameLogic.RandCardList(m_cbTableCard,CountArray(m_cbTableCard));	

		//设置扑克
		CopyMemory(&m_cbTableCardArray[0][0], m_cbTableCard, sizeof(m_cbTableCardArray));

//#ifdef DEBUG  
//		{
//
//			BYTE temp1[5]={0x11,0x1A,0x3D,0x12,0x05};
//			BYTE temp2[5]={0x41,0x3a,0x2c,0x08,0x03};
//			BYTE temp3[5]={0x2A,0x0C,0x18,0x33,0x31};
//			BYTE temp4[5]={0x27,0x03,0x42,0x0D,0x07};
//			BYTE temp5[5]={0x16,0x12,0x1C,0x3B,0x3A};
//			CopyMemory(&m_cbTableCardArray[0][0], temp1, sizeof(m_cbTableCardArray[0]));
//			CopyMemory(&m_cbTableCardArray[1][0], temp2, sizeof(m_cbTableCardArray[1]));
//			CopyMemory(&m_cbTableCardArray[2][0], temp3, sizeof(m_cbTableCardArray[2]));
//			CopyMemory(&m_cbTableCardArray[3][0], temp4, sizeof(m_cbTableCardArray[3]));
//			CopyMemory(&m_cbTableCardArray[4][0], temp5, sizeof(m_cbTableCardArray[3]));
//
//		}
//#endif

		//庄家判断
		if (INVALID_CHAIR != m_wCurrentBanker)
		{
			m_cbLeftCardCount=CountArray(m_cbTableCard)-nDispatchCardCount;
			CopyMemory(m_cbTableCard, m_cbTableCard + nDispatchCardCount, sizeof(BYTE)*m_cbLeftCardCount);
		}
		else
		{
			m_cbLeftCardCount=0;
		}
	}

	//设置数目
	for (int i=0; i<CountArray(m_cbCardCount); ++i) m_cbCardCount[i]=5;

	//发牌标志
	m_bContiueCard=false;

	return true;
}

//申请庄家
bool CTableFrameSink::OnUserApplyBanker(IServerUserItem *pIApplyServerUserItem)
{
	//合法判断
	LONGLONG lUserScore=pIApplyServerUserItem->GetUserScore();
	if (lUserScore<m_lApplyBankerCondition)
	{
		m_pITableFrame->SendGameMessage(pIApplyServerUserItem,TEXT("你的金币不足以申请庄家，申请失败！"),SMT_CHAT|SMT_EJECT);
		return true;
	}

	//存在判断
	WORD wApplyUserChairID=pIApplyServerUserItem->GetChairID();
	for (INT_PTR nUserIdx=0; nUserIdx<m_ApplyUserArray.GetCount(); ++nUserIdx)
	{
		WORD wChairID=m_ApplyUserArray[nUserIdx];
		if (wChairID==wApplyUserChairID)
		{
			m_pITableFrame->SendGameMessage(pIApplyServerUserItem,TEXT("你已经申请了庄家，不需要再次申请！"),SMT_CHAT|SMT_EJECT);
			return true;
		}
	}

	if (pIApplyServerUserItem->IsAndroidUser()&&(m_ApplyUserArray.GetCount())>m_nRobotListMaxCount)
	{
		return true;
	}

	//保存信息 
	m_ApplyUserArray.Add(wApplyUserChairID);

	//构造变量
	CMD_S_ApplyBanker ApplyBanker;
	ZeroMemory(&ApplyBanker,sizeof(ApplyBanker));

	//设置变量
	ApplyBanker.wApplyUser=wApplyUserChairID;

	//发送消息
	m_pITableFrame->SendTableData(INVALID_CHAIR, SUB_S_APPLY_BANKER, &ApplyBanker, sizeof(ApplyBanker));
	m_pITableFrame->SendLookonData(INVALID_CHAIR, SUB_S_APPLY_BANKER, &ApplyBanker, sizeof(ApplyBanker));

	//切换判断
	if (m_pITableFrame->GetGameStatus()==GAME_SCENE_FREE && m_ApplyUserArray.GetCount()==1)
	{
		ChangeBanker(false);
	}

	return true;
}

//取消申请
bool CTableFrameSink::OnUserCancelBanker(IServerUserItem *pICancelServerUserItem)
{
	//当前庄家
	if (pICancelServerUserItem->GetChairID()==m_wCurrentBanker && m_pITableFrame->GetGameStatus()!=GAME_SCENE_FREE)
	{
		//发送消息
		m_pITableFrame->SendGameMessage(pICancelServerUserItem,TEXT("游戏已经开始，不可以取消当庄！"),SMT_CHAT|SMT_EJECT);
		return true;
	}
	
	bool bValidQuitBanker = true;
	//存在判断
	for (WORD i=0; i<m_ApplyUserArray.GetCount(); ++i)
	{
		//获取玩家
		WORD wChairID=m_ApplyUserArray[i];
		IServerUserItem *pIServerUserItem=m_pITableFrame->GetTableUserItem(wChairID);

		//条件过滤
		if (pIServerUserItem==NULL) continue;
		if (pIServerUserItem->GetUserID()!=pICancelServerUserItem->GetUserID()) continue;

		//删除玩家
		m_ApplyUserArray.RemoveAt(i);

		bValidQuitBanker = false;

		//超级抢庄玩家
		if (wChairID == m_wCurSuperRobBankerUser)
		{
			m_wCurSuperRobBankerUser = INVALID_CHAIR;
		}

		if (m_wCurrentBanker!=wChairID)
		{
			//构造变量
			CMD_S_CancelBanker CancelBanker;
			ZeroMemory(&CancelBanker,sizeof(CancelBanker));

			//设置变量
			CancelBanker.wCancelUser = pIServerUserItem->GetChairID();

			//发送消息
			m_pITableFrame->SendTableData(INVALID_CHAIR, SUB_S_CANCEL_BANKER, &CancelBanker, sizeof(CancelBanker));
			m_pITableFrame->SendLookonData(INVALID_CHAIR, SUB_S_CANCEL_BANKER, &CancelBanker, sizeof(CancelBanker));
		}
		else if (m_wCurrentBanker==wChairID)
		{
			//切换庄家 
			m_wCurrentBanker=INVALID_CHAIR;
			m_typeCurrentBanker = INVALID_SYSBANKER;
			ChangeBanker(true);
		}

		return true;
	}
	
	if (bValidQuitBanker == true)
	{
		m_wCurrentBanker=INVALID_CHAIR;
		m_typeCurrentBanker = INVALID_SYSBANKER;
		ChangeBanker(true);
		return true;
	}

	return true;
}

//用户占位
bool CTableFrameSink::OnUserOccupySeat(WORD wOccupyChairID, BYTE cbOccupySeatIndex)
{
	//校验用户
	ASSERT (wOccupyChairID != INVALID_CHAIR);
	if (wOccupyChairID == INVALID_CHAIR)
	{
		return true;
	}

	ASSERT (cbOccupySeatIndex != SEAT_INVALID_INDEX);

	ASSERT (cbOccupySeatIndex >= SEAT_LEFT1_INDEX && cbOccupySeatIndex <= SEAT_RIGHT3_INDEX);
	if (!(cbOccupySeatIndex >= SEAT_LEFT1_INDEX && cbOccupySeatIndex <= SEAT_RIGHT3_INDEX))
	{
		return true;
	}

	if (m_wOccupySeatChairIDArray[cbOccupySeatIndex] != INVALID_CHAIR) 
	{
		return true;
	}
	ASSERT (m_wOccupySeatChairIDArray[cbOccupySeatIndex] == INVALID_CHAIR);

	//校验是否已经占位
	for (WORD i=0; i<MAX_OCCUPY_SEAT_COUNT; i++)
	{
		//占位失败
		if (m_wOccupySeatChairIDArray[i] == wOccupyChairID)
		{
			CMD_S_OccupySeat_Fail OccupySeat_Fail;
			ZeroMemory(&OccupySeat_Fail, sizeof(OccupySeat_Fail));
			CopyMemory(OccupySeat_Fail.wOccupySeatChairIDArray, m_wOccupySeatChairIDArray, sizeof(m_wOccupySeatChairIDArray));
			OccupySeat_Fail.wAlreadyOccupySeatChairID = wOccupyChairID;
			OccupySeat_Fail.cbAlreadyOccupySeatIndex = i;

			//发送数据
			m_pITableFrame->SendTableData(wOccupyChairID, SUB_S_OCCUPYSEAT_FAIL, &OccupySeat_Fail, sizeof(OccupySeat_Fail));
			m_pITableFrame->SendLookonData(wOccupyChairID, SUB_S_OCCUPYSEAT_FAIL, &OccupySeat_Fail, sizeof(OccupySeat_Fail));

			return true;
		}
	}

	//占位VIP类型
	if (m_occupyseatConfig.occupyseatType == OCCUPYSEAT_VIPTYPE)
	{
		ASSERT (m_pITableFrame->GetTableUserItem(wOccupyChairID)->GetMemberOrder() >= m_occupyseatConfig.enVipIndex);
		if (m_pITableFrame->GetTableUserItem(wOccupyChairID)->GetMemberOrder() < m_occupyseatConfig.enVipIndex)
		{
			return true;
		}
	}
	else if (m_occupyseatConfig.occupyseatType == OCCUPYSEAT_CONSUMETYPE)
	{
		ASSERT (m_pITableFrame->GetTableUserItem(wOccupyChairID)->GetUserScore() >= m_occupyseatConfig.lOccupySeatConsume);
		if (m_pITableFrame->GetTableUserItem(wOccupyChairID)->GetUserScore() < m_occupyseatConfig.lOccupySeatConsume)
		{
			return true;
		}

		//占位消耗
		tagScoreInfo ScoreInfoArray[GAME_PLAYER];
		ZeroMemory(ScoreInfoArray, sizeof(ScoreInfoArray));
		ScoreInfoArray[wOccupyChairID].cbType = SCORE_TYPE_DRAW;
		ScoreInfoArray[wOccupyChairID].lRevenue = 0;  
		ScoreInfoArray[wOccupyChairID].lScore = -m_occupyseatConfig.lOccupySeatConsume;

		//占位写分
		m_pITableFrame->WriteTableScore(ScoreInfoArray, CountArray(ScoreInfoArray));
	}
	else if (m_occupyseatConfig.occupyseatType == OCCUPYSEAT_FREETYPE)
	{
		ASSERT (m_pITableFrame->GetTableUserItem(wOccupyChairID)->GetUserScore() >= m_occupyseatConfig.lOccupySeatFree);
		if (m_pITableFrame->GetTableUserItem(wOccupyChairID)->GetUserScore() < m_occupyseatConfig.lOccupySeatFree)
		{
			return true;
		}
	}

	//占位成功
	m_wOccupySeatChairIDArray[cbOccupySeatIndex] = wOccupyChairID;

	CMD_S_OccupySeat OccupySeat;
	ZeroMemory(&OccupySeat, sizeof(OccupySeat));
	CopyMemory(OccupySeat.wOccupySeatChairIDArray, m_wOccupySeatChairIDArray, sizeof(m_wOccupySeatChairIDArray));
	OccupySeat.wOccupySeatChairID = wOccupyChairID;
	OccupySeat.cbOccupySeatIndex = cbOccupySeatIndex;

	//发送数据
	m_pITableFrame->SendTableData(INVALID_CHAIR, SUB_S_OCCUPYSEAT, &OccupySeat, sizeof(OccupySeat));
	m_pITableFrame->SendLookonData(INVALID_CHAIR, SUB_S_OCCUPYSEAT, &OccupySeat, sizeof(OccupySeat));

	return true;
}

//更换庄家
bool CTableFrameSink::ChangeBanker(bool bCancelCurrentBanker)
{
	//切换标识
	bool bChangeBanker=false;

	//取消当前
	if (bCancelCurrentBanker)
	{
		for (WORD i=0; i<m_ApplyUserArray.GetCount(); ++i)
		{
			//获取玩家
			WORD wChairID=m_ApplyUserArray[i];

			//条件过滤
			if (wChairID!=m_wCurrentBanker) continue;

			//删除玩家
			m_ApplyUserArray.RemoveAt(i);

			break;
		}

		//设置庄家
		m_wCurrentBanker=INVALID_CHAIR;

		m_typeCurrentBanker = INVALID_SYSBANKER;

		//轮换判断
		TakeTurns();

		if (m_wCurSuperRobBankerUser != INVALID_CHAIR && m_wCurrentBanker != INVALID_CHAIR)
		{
			ASSERT (m_wCurSuperRobBankerUser == m_wCurrentBanker);
			m_wCurSuperRobBankerUser = INVALID_CHAIR;
			m_typeCurrentBanker = SUPERROB_BANKER;
		}
		else if (m_wCurSuperRobBankerUser == INVALID_CHAIR && m_wCurrentBanker != INVALID_CHAIR)
		{
			m_typeCurrentBanker = ORDINARY_BANKER;
		}
		else if (m_wCurSuperRobBankerUser == INVALID_CHAIR && m_wCurrentBanker == INVALID_CHAIR)
		{
			m_typeCurrentBanker = INVALID_SYSBANKER;
		}

		//设置变量
		bChangeBanker=true;
		m_bExchangeBanker = true;
	}
	//轮庄判断
	else if (m_wCurrentBanker!=INVALID_CHAIR)
	{
		if (m_wCurSuperRobBankerUser != INVALID_CHAIR && m_typeCurrentBanker == ORDINARY_BANKER)
		{
			//轮换判断
			TakeTurns();

			if (m_wCurSuperRobBankerUser != INVALID_CHAIR && m_wCurrentBanker != INVALID_CHAIR)
			{
				m_wCurSuperRobBankerUser = INVALID_CHAIR;
				m_typeCurrentBanker = SUPERROB_BANKER;
			}
			else if (m_wCurSuperRobBankerUser == INVALID_CHAIR && m_wCurrentBanker != INVALID_CHAIR)
			{
				m_typeCurrentBanker = ORDINARY_BANKER;
			}
			else if (m_wCurSuperRobBankerUser == INVALID_CHAIR && m_wCurrentBanker == INVALID_CHAIR)
			{
				m_typeCurrentBanker = INVALID_SYSBANKER;
			}

			//撤销玩家
			for (WORD i=0; i<m_ApplyUserArray.GetCount(); ++i)
			{
				//获取玩家
				WORD wChairID = m_ApplyUserArray[i];

				//条件过滤
				if (wChairID != m_wCurrentBanker)
				{
					continue;
				}

				//删除玩家
				m_ApplyUserArray.RemoveAt(i);

				break;
			}

			bChangeBanker=true;
			m_bExchangeBanker = true;
		}
		else
		{
			//获取庄家
			IServerUserItem *pIServerUserItem=m_pITableFrame->GetTableUserItem(m_wCurrentBanker);
			if(pIServerUserItem)
			{
				LONGLONG lBankerScore=pIServerUserItem->GetUserScore();

				//次数判断
				if (m_lPlayerBankerMAX<=m_wBankerTime || lBankerScore<m_lApplyBankerCondition)
				{
					//庄家增加判断 同一个庄家情况下只判断一次
					if(m_lPlayerBankerMAX <= m_wBankerTime && m_bExchangeBanker && lBankerScore>=m_lApplyBankerCondition)
					{
						//加庄局数设置：当庄家坐满设定的局数之后(m_nBankerTimeLimit)，
						//所带金币值还超过下面申请庄家列表里面所有玩家金币时，
						//可以再加坐庄m_nBankerTimeAdd局，加庄局数可设置。

						//金币超过m_lExtraBankerScore之后，
						//就算是下面玩家的金币值大于他的金币值，他也可以再加庄m_nExtraBankerTime局。
						bool bScoreMAX = true;
						m_bExchangeBanker = false;

						for (WORD i=0; i<m_ApplyUserArray.GetCount(); ++i)
						{
							//获取玩家
							WORD wChairID = m_ApplyUserArray[i];
							IServerUserItem *pIUserItem = m_pITableFrame->GetTableUserItem(wChairID);
							LONGLONG lScore = pIServerUserItem->GetUserScore();

							if ( wChairID != m_wCurrentBanker && lBankerScore <= lScore )
							{
								bScoreMAX = false;
								break;
							}
						}

						if ( bScoreMAX || (lBankerScore > m_lExtraBankerScore && m_lExtraBankerScore != 0l) )
						{
							if ( bScoreMAX )
							{
								m_lPlayerBankerMAX += m_nBankerTimeAdd;
							}
							if ( lBankerScore > m_lExtraBankerScore && m_lExtraBankerScore != 0l )
							{
								m_lPlayerBankerMAX += m_nExtraBankerTime;
							}
							return true;
						}
					}

					//设置庄家
					m_wCurrentBanker=INVALID_CHAIR;

					m_typeCurrentBanker = INVALID_SYSBANKER;

					//轮换判断
					TakeTurns();

					if (m_wCurSuperRobBankerUser != INVALID_CHAIR && m_wCurrentBanker != INVALID_CHAIR)
					{
						m_wCurSuperRobBankerUser = INVALID_CHAIR;
						m_typeCurrentBanker = SUPERROB_BANKER;
					}
					else if (m_wCurSuperRobBankerUser == INVALID_CHAIR && m_wCurrentBanker != INVALID_CHAIR)
					{
						m_typeCurrentBanker = ORDINARY_BANKER;
					}
					else if (m_wCurSuperRobBankerUser == INVALID_CHAIR && m_wCurrentBanker == INVALID_CHAIR)
					{
						m_typeCurrentBanker = INVALID_SYSBANKER;
					}

					//撤销玩家
					for (WORD i=0; i<m_ApplyUserArray.GetCount(); ++i)
					{
						//获取玩家
						WORD wChairID=m_ApplyUserArray[i];

						//条件过滤
						if (wChairID!=m_wCurrentBanker) continue;

						//删除玩家
						m_ApplyUserArray.RemoveAt(i);

						break;
					}

					bChangeBanker=true;
					m_bExchangeBanker = true;

					//提示消息
					TCHAR szTipMsg[128];
					if (lBankerScore<m_lApplyBankerCondition)
						_sntprintf(szTipMsg,CountArray(szTipMsg),TEXT("[ %s ]分数少于(%I64d)，强行换庄!"),pIServerUserItem->GetNickName(),m_lApplyBankerCondition);
					else
						_sntprintf(szTipMsg,CountArray(szTipMsg),TEXT("[ %s ]做庄次数达到(%d)，强行换庄!"),pIServerUserItem->GetNickName(),m_lPlayerBankerMAX);

					//发送消息
					SendGameMessage(INVALID_CHAIR,szTipMsg);	
				}
			}
		}
	}
	//系统做庄
	else if (m_wCurrentBanker == INVALID_CHAIR && m_ApplyUserArray.GetCount()!=0)
	{
		//轮换判断
		TakeTurns();

		if (m_wCurSuperRobBankerUser != INVALID_CHAIR && m_wCurrentBanker != INVALID_CHAIR)
		{
			ASSERT (m_wCurSuperRobBankerUser == m_wCurrentBanker);
			m_wCurSuperRobBankerUser = INVALID_CHAIR;
			m_typeCurrentBanker = SUPERROB_BANKER;
		}
		else if (m_wCurSuperRobBankerUser == INVALID_CHAIR && m_wCurrentBanker != INVALID_CHAIR)
		{
			m_typeCurrentBanker = ORDINARY_BANKER;
		}

		//撤销玩家
		for (WORD i=0; i<m_ApplyUserArray.GetCount(); ++i)
		{
			//获取玩家
			WORD wChairID=m_ApplyUserArray[i];

			//条件过滤
			if (wChairID!=m_wCurrentBanker) continue;

			//删除玩家
			m_ApplyUserArray.RemoveAt(i);

			break;
		}

		bChangeBanker=true;
		m_bExchangeBanker = true;
	}

	//切换判断
	if (bChangeBanker)
	{
		//最大坐庄数
		m_lPlayerBankerMAX = m_nBankerTimeLimit;

		//设置变量
		m_wBankerTime = 0;
		m_lBankerWinScore=0;

		//发送消息
		CMD_S_ChangeBanker stChangeBanker;
		ZeroMemory(&stChangeBanker,sizeof(stChangeBanker));
		stChangeBanker.wBankerUser = m_wCurrentBanker;
		if (m_wCurrentBanker != INVALID_CHAIR)
		{
			IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(m_wCurrentBanker);
			stChangeBanker.lBankerScore = pIServerUserItem->GetUserScore();
		}
		else
		{
			stChangeBanker.lBankerScore = 1000000000;
		}

		stChangeBanker.typeCurrentBanker = m_typeCurrentBanker;
		m_pITableFrame->SendTableData(INVALID_CHAIR,SUB_S_CHANGE_BANKER,&stChangeBanker,sizeof(CMD_S_ChangeBanker));
		m_pITableFrame->SendLookonData(INVALID_CHAIR,SUB_S_CHANGE_BANKER,&stChangeBanker,sizeof(CMD_S_ChangeBanker));

		if (m_wCurrentBanker!=INVALID_CHAIR)
		{
			//获取参数
			m_pGameServiceOption=m_pITableFrame->GetGameServiceOption();
			ASSERT(m_pGameServiceOption!=NULL);

			//读取消息
			int nMessageCount = 3;

			//读取配置
			INT nIndex=rand()%nMessageCount;
			TCHAR szMessage1[256],szMessage2[256];
			tagCustomConfig *pCustomConfig = (tagCustomConfig *)m_pGameServiceOption->cbCustomRule;
			ASSERT(pCustomConfig);
			if(0 == nIndex)
			{
				CopyMemory(szMessage1, pCustomConfig->CustomGeneral.szMessageItem1, sizeof(pCustomConfig->CustomGeneral.szMessageItem1));
			}
			else if(1 == nIndex)
			{
				CopyMemory(szMessage1, pCustomConfig->CustomGeneral.szMessageItem2, sizeof(pCustomConfig->CustomGeneral.szMessageItem2));
			}
			else if(2 == nIndex)
			{
				CopyMemory(szMessage1, pCustomConfig->CustomGeneral.szMessageItem3, sizeof(pCustomConfig->CustomGeneral.szMessageItem3));
			}

			//获取玩家
			IServerUserItem *pIServerUserItem=m_pITableFrame->GetTableUserItem(m_wCurrentBanker);

			//发送消息
			_sntprintf(szMessage2,CountArray(szMessage2),TEXT("【 %s 】上庄了，%s"),pIServerUserItem->GetNickName(), szMessage1);
			SendGameMessage(INVALID_CHAIR,szMessage2);
		}
	}

	return bChangeBanker;
}

//轮换判断
void CTableFrameSink::TakeTurns()
{
	//变量定义
	int nInvalidApply = 0;

	for (int i = 0; i < m_ApplyUserArray.GetCount(); i++)
	{
		if (m_pITableFrame->GetGameStatus() == GAME_SCENE_FREE)
		{
			//获取分数
			IServerUserItem *pIServerUserItem=m_pITableFrame->GetTableUserItem(m_ApplyUserArray[i]);
			if (pIServerUserItem != NULL)
			{
				if (pIServerUserItem->GetUserScore() >= m_lApplyBankerCondition)
				{
					m_wCurrentBanker=m_ApplyUserArray[i];
					break;
				}
				else
				{
					nInvalidApply = i + 1;

					//发送消息
					CMD_S_CancelBanker CancelBanker = {};

					//设置变量
					CancelBanker.wCancelUser=pIServerUserItem->GetChairID();					

					//发送消息
					m_pITableFrame->SendTableData(INVALID_CHAIR, SUB_S_CANCEL_BANKER, &CancelBanker, sizeof(CancelBanker));
					m_pITableFrame->SendLookonData(INVALID_CHAIR, SUB_S_CANCEL_BANKER, &CancelBanker, sizeof(CancelBanker));

					//提示消息
					TCHAR szTipMsg[128] = {};
					_sntprintf(szTipMsg,CountArray(szTipMsg),TEXT("由于你的金币数（%I64d）少于坐庄必须金币数（%I64d）,你无法上庄！"),
						pIServerUserItem->GetUserScore(), m_lApplyBankerCondition);
					SendGameMessage(m_ApplyUserArray[i],szTipMsg);
				}
			}
		}
	}

	//删除玩家
	if (nInvalidApply != 0)
		m_ApplyUserArray.RemoveAt(0, nInvalidApply);
}

//发送庄家
void CTableFrameSink::SendApplyUser( IServerUserItem *pRcvServerUserItem )
{
	for (INT_PTR nUserIdx=0; nUserIdx<m_ApplyUserArray.GetCount(); ++nUserIdx)
	{
		WORD wChairID=m_ApplyUserArray[nUserIdx];

		//获取玩家
		IServerUserItem *pServerUserItem=m_pITableFrame->GetTableUserItem(wChairID);
		if (!pServerUserItem) continue;

		//庄家判断
		if (pServerUserItem->GetChairID()==m_wCurrentBanker) continue;

		//构造变量
		CMD_S_ApplyBanker ApplyBanker;
		ApplyBanker.wApplyUser=wChairID;

		//发送消息
		m_pITableFrame->SendUserItemData(pRcvServerUserItem, SUB_S_APPLY_BANKER, &ApplyBanker, sizeof(ApplyBanker));
	}
}


//最大下注
LONGLONG CTableFrameSink::GetUserMaxJetton(WORD wChairID, BYTE cbJettonArea)
{
	IServerUserItem *pIMeServerUserItem=m_pITableFrame->GetTableUserItem(wChairID);
	if (NULL==pIMeServerUserItem) return 0L;

	int iTimer = 10;
	//已下注额
	LONGLONG lNowJetton = 0;
	ASSERT(AREA_COUNT<=CountArray(m_lUserJettonScore));
	for (int nAreaIndex=1; nAreaIndex<=AREA_COUNT; ++nAreaIndex) lNowJetton += m_lUserJettonScore[nAreaIndex][wChairID];

	//庄家金币
	LONGLONG lBankerScore=m_lBankerScore*m_nEndGameMul/100;;
	for (int nAreaIndex=1; nAreaIndex<=AREA_COUNT; ++nAreaIndex) lBankerScore-=m_lAllJettonScore[nAreaIndex]*iTimer;

	//区域金币
	LONGLONG lAreaLimitScore = m_lAreaLimitScore - m_lAllJettonScore[cbJettonArea];

	//个人限制
	LONGLONG lMeMaxScore = min((pIMeServerUserItem->GetUserScore()-lNowJetton*iTimer)/iTimer, m_lUserLimitScore);

//	const tagUserScore *Info = pIMeServerUserItem->GetUserScore();

//	const tagUserInfo *Data = pIMeServerUserItem->GetUserInfo();

	//区域限制
	lMeMaxScore=min(lMeMaxScore,lAreaLimitScore);

	//庄家限制
	lMeMaxScore=min(lMeMaxScore,lBankerScore/iTimer);

	//非零限制
	ASSERT(lMeMaxScore >= 0);
	lMeMaxScore = max(lMeMaxScore, 0);

	return (lMeMaxScore);

}
//计算得分
LONGLONG CTableFrameSink::CalculateScore()
{
	//	return 1;
	//变量定义
	LONGLONG static wRevenue=m_pGameServiceOption->wRevenueRatio;

	//推断玩家
	bool static bWinTianMen, bWinDiMen, bWinXuanMen,bWinHuang;
	BYTE TianMultiple,diMultiple,TianXuanltiple,HuangMultiple;
	TianMultiple  = 1;
	diMultiple = 1 ;
	TianXuanltiple = 1;
	HuangMultiple = 1;
	DeduceWinner(bWinTianMen, bWinDiMen, bWinXuanMen,bWinHuang,TianMultiple,diMultiple,TianXuanltiple,HuangMultiple );

	//游戏记录
	tagServerGameRecord &GameRecord = m_GameRecordArrary[m_nRecordLast];
	GameRecord.bWinShunMen=bWinTianMen;
	GameRecord.bWinDuiMen=bWinDiMen;
	GameRecord.bWinDaoMen=bWinXuanMen;
	GameRecord.bWinHuang=bWinHuang;

	BYTE  cbMultiple[] = {0, 1, 1, 1, 1, 1, 1};

	cbMultiple[1] = TianMultiple;
	cbMultiple[2] = diMultiple;
	cbMultiple[3] = TianXuanltiple;
	cbMultiple[4] = HuangMultiple;

	//移动下标
	m_nRecordLast = (m_nRecordLast+1) % MAX_SCORE_HISTORY;
	if ( m_nRecordLast == m_nRecordFirst ) m_nRecordFirst = (m_nRecordFirst+1) % MAX_SCORE_HISTORY;

	//庄家总量
	LONGLONG lBankerWinScore = 0;

	//玩家成绩
	ZeroMemory(m_lUserWinScore, sizeof(m_lUserWinScore));
	ZeroMemory(m_lUserReturnScore, sizeof(m_lUserReturnScore));
	ZeroMemory(m_lUserRevenue, sizeof(m_lUserRevenue));
	LONGLONG lUserLostScore[GAME_PLAYER];
	ZeroMemory(lUserLostScore, sizeof(lUserLostScore));

	//玩家下注
	LONGLONG *const pUserScore[] = {NULL,m_lUserJettonScore[ID_TIAN_MEN], m_lUserJettonScore[ID_DI_MEN], m_lUserJettonScore[ID_XUAN_MEN],
		m_lUserJettonScore[ID_HUANG_MEN]};	

	//区域倍率
	//BYTE static const cbMultiple[] = {0, 1, 1, 1, 1, 1, 1};

	//胜利标识
	bool static bWinFlag[AREA_COUNT+1];
	bWinFlag[0]=false;
	bWinFlag[ID_TIAN_MEN]=bWinTianMen;
	bWinFlag[ID_DI_MEN]=bWinDiMen;
	bWinFlag[ID_XUAN_MEN]=bWinXuanMen;
	bWinFlag[ID_HUANG_MEN]=bWinHuang;

	//FILE *pf = fopen("C://ServLong.txt","ab+");

	//计算积分
	for (WORD wChairID=0; wChairID<GAME_PLAYER; wChairID++)
	{
		//庄家判断
		if (m_wCurrentBanker==wChairID) continue;

		//获取用户
		IServerUserItem * pIServerUserItem=m_pITableFrame->GetTableUserItem(wChairID);
		if (pIServerUserItem==NULL) continue;

		for (WORD wAreaIndex = ID_TIAN_MEN; wAreaIndex <= ID_HUANG_MEN; ++wAreaIndex)
		{

			if (true==bWinFlag[wAreaIndex]) 
			{
				m_lUserWinScore[wChairID] += ( pUserScore[wAreaIndex][wChairID] * cbMultiple[wAreaIndex] ) ;
				m_lUserReturnScore[wChairID] += pUserScore[wAreaIndex][wChairID] ;
				lBankerWinScore -= ( pUserScore[wAreaIndex][wChairID] * cbMultiple[wAreaIndex] ) ;
			}
			else
			{
				lUserLostScore[wChairID] -= pUserScore[wAreaIndex][wChairID]*cbMultiple[wAreaIndex];
				lBankerWinScore += pUserScore[wAreaIndex][wChairID]*cbMultiple[wAreaIndex];
			}
		}
		
		//总的分数
		m_lUserWinScore[wChairID] += lUserLostScore[wChairID];

		//计算税收
		if (0 < m_lUserWinScore[wChairID])
		{
			double fRevenuePer=double(wRevenue/1000.0);
			m_lUserRevenue[wChairID]  = LONGLONG(m_lUserWinScore[wChairID]*fRevenuePer);
			m_lUserWinScore[wChairID] -= m_lUserRevenue[wChairID];
		}
	}

	//庄家成绩
	if (m_wCurrentBanker!=INVALID_CHAIR)
	{
		m_lUserWinScore[m_wCurrentBanker] = lBankerWinScore;

		//计算税收
		if (0 < m_lUserWinScore[m_wCurrentBanker])
		{
			double fRevenuePer=double(wRevenue/1000.0);
			m_lUserRevenue[m_wCurrentBanker]  = LONGLONG(m_lUserWinScore[m_wCurrentBanker]*fRevenuePer);
			m_lUserWinScore[m_wCurrentBanker] -= m_lUserRevenue[m_wCurrentBanker];
			lBankerWinScore = m_lUserWinScore[m_wCurrentBanker];
		}	
	}

	//离线庄家成绩
	if (m_wOfflineBanker!=INVALID_CHAIR)
	{
		m_lUserWinScore[m_wOfflineBanker] = lBankerWinScore;

		//计算税收
		if (0 < m_lUserWinScore[m_wOfflineBanker])
		{
			double fRevenuePer=double(wRevenue/1000.0);
			m_lUserRevenue[m_wOfflineBanker]  = LONGLONG(m_lUserWinScore[m_wOfflineBanker]*fRevenuePer);
			m_lUserWinScore[m_wOfflineBanker] -= m_lUserRevenue[m_wOfflineBanker];
			lBankerWinScore = m_lUserWinScore[m_wOfflineBanker];
		}	
	}

	//累计积分
	m_lBankerWinScore += lBankerWinScore;

	//当前积分
	m_lBankerCurGameScore=lBankerWinScore;

	return lBankerWinScore;
}


void CTableFrameSink::DeduceWinner(bool &bWinTian, bool &bWinDi, bool &bWinXuan,bool &bWinHuan,BYTE &TianMultiple,BYTE &diMultiple,BYTE &TianXuanltiple,BYTE &HuangMultiple)
{
	//大小比较
	bWinTian=m_GameLogic.CompareCard(m_cbTableCardArray[BANKER_INDEX],5,m_cbTableCardArray[SHUN_MEN_INDEX],5,TianMultiple)==1?true:false;
	bWinDi=m_GameLogic.CompareCard(m_cbTableCardArray[BANKER_INDEX],5,m_cbTableCardArray[DUI_MEN_INDEX],5,diMultiple)==1?true:false;
	bWinXuan=m_GameLogic.CompareCard(m_cbTableCardArray[BANKER_INDEX],5,m_cbTableCardArray[DAO_MEN_INDEX],5,TianXuanltiple)==1?true:false;
	bWinHuan=m_GameLogic.CompareCard(m_cbTableCardArray[BANKER_INDEX],5,m_cbTableCardArray[HUAN_MEN_INDEX],5,HuangMultiple)==1?true:false;
}

//获取牌信息
CString CTableFrameSink::GetCardInfo(BYTE cbCardData)
{
	CString strInfo;

	if (cbCardData == 0x41)
	{
		strInfo += TEXT("[小王 ");
	}
	else if (cbCardData == 0x42)
	{
		strInfo += TEXT("[大王 ");
	}
	else
	{
		if( (cbCardData&LOGIC_MASK_COLOR) == 0x00 )
			strInfo += TEXT("[方块 ");
		else if( (cbCardData&LOGIC_MASK_COLOR) == 0x10 )
			strInfo += TEXT("[梅花 ");
		else if( (cbCardData&LOGIC_MASK_COLOR) == 0x20 )
			strInfo += TEXT("[红桃 ");
		else if( (cbCardData&LOGIC_MASK_COLOR) == 0x30 )
			strInfo += TEXT("[黑桃 ");

		if( (cbCardData&LOGIC_MASK_VALUE) == 0x01 )
			strInfo += TEXT("A] ");
		else if( (cbCardData&LOGIC_MASK_VALUE) == 0x02 )
			strInfo += TEXT("2] ");
		else if( (cbCardData&LOGIC_MASK_VALUE) == 0x03 )
			strInfo += TEXT("3] ");
		else if( (cbCardData&LOGIC_MASK_VALUE) == 0x04 )
			strInfo += TEXT("4] ");
		else if( (cbCardData&LOGIC_MASK_VALUE) == 0x05 )
			strInfo += TEXT("5] ");
		else if( (cbCardData&LOGIC_MASK_VALUE) == 0x06 )
			strInfo += TEXT("6] ");
		else if( (cbCardData&LOGIC_MASK_VALUE) == 0x07 )
			strInfo += TEXT("7] ");
		else if( (cbCardData&LOGIC_MASK_VALUE) == 0x08 )
			strInfo += TEXT("8] ");
		else if( (cbCardData&LOGIC_MASK_VALUE) == 0x09 )
			strInfo += TEXT("9] ");
		else if( (cbCardData&LOGIC_MASK_VALUE) == 0x0A )
			strInfo += TEXT("10] ");
		else if( (cbCardData&LOGIC_MASK_VALUE) == 0x0B )
			strInfo += TEXT("J] ");
		else if( (cbCardData&LOGIC_MASK_VALUE) == 0x0C )
			strInfo += TEXT("Q] ");
		else if( (cbCardData&LOGIC_MASK_VALUE) == 0x0D )
			strInfo += TEXT("K] ");
	}

	return strInfo;
}

//发送记录
void CTableFrameSink::SendGameRecord(IServerUserItem *pIServerUserItem)
{
	WORD wBufferSize=0;
	BYTE cbBuffer[8192];
	int nIndex = m_nRecordFirst;
	while ( nIndex != m_nRecordLast )
	{
		if ((wBufferSize+sizeof(tagServerGameRecord))>sizeof(cbBuffer))
		{
			m_pITableFrame->SendUserItemData(pIServerUserItem,SUB_S_SEND_RECORD,cbBuffer,wBufferSize);
			wBufferSize=0;
		}
		CopyMemory(cbBuffer+wBufferSize,&m_GameRecordArrary[nIndex],sizeof(tagServerGameRecord));
		wBufferSize+=sizeof(tagServerGameRecord);

		nIndex = (nIndex+1) % MAX_SCORE_HISTORY;
	}
	if (wBufferSize>0) m_pITableFrame->SendUserItemData(pIServerUserItem,SUB_S_SEND_RECORD,cbBuffer,wBufferSize);
}

//发送消息
void CTableFrameSink::SendGameMessage(WORD wChairID, LPCTSTR pszTipMsg)
{
	if (wChairID==INVALID_CHAIR)
	{
		//游戏玩家
		for (WORD i=0; i<GAME_PLAYER; ++i)
		{
			IServerUserItem *pIServerUserItem=m_pITableFrame->GetTableUserItem(i);
			if (pIServerUserItem==NULL) continue;
			m_pITableFrame->SendGameMessage(pIServerUserItem,pszTipMsg,SMT_CHAT);
		}

		//旁观玩家
		WORD wIndex=0;
		do {
			IServerUserItem *pILookonServerUserItem=m_pITableFrame->EnumLookonUserItem(wIndex++);
			if (pILookonServerUserItem==NULL) break;

			m_pITableFrame->SendGameMessage(pILookonServerUserItem,pszTipMsg,SMT_CHAT);

		}while(true);
	}
	else
	{
		IServerUserItem *pIServerUserItem=m_pITableFrame->GetTableUserItem(wChairID);
		if (pIServerUserItem!=NULL) m_pITableFrame->SendGameMessage(pIServerUserItem,pszTipMsg,SMT_CHAT|SMT_EJECT);
	}
}

//读取配置
void CTableFrameSink::ReadConfigInformation()
{
	//获取自定义配置
	tagCustomConfig *pCustomConfig = (tagCustomConfig *)m_pGameServiceOption->cbCustomRule;
	ASSERT(pCustomConfig);

	//上庄
	m_lApplyBankerCondition = pCustomConfig->CustomGeneral.lApplyBankerCondition;
	m_nBankerTimeLimit = pCustomConfig->CustomGeneral.lBankerTime;
	m_nBankerTimeAdd = pCustomConfig->CustomGeneral.lBankerTimeAdd;
	m_lExtraBankerScore = pCustomConfig->CustomGeneral.lBankerScoreMAX;
	m_nExtraBankerTime = pCustomConfig->CustomGeneral.lBankerTimeExtra;
	m_bEnableSysBanker = (pCustomConfig->CustomGeneral.nEnableSysBanker == TRUE)?true:false;
	
	//超级抢庄
	CopyMemory(&m_superbankerConfig, &(pCustomConfig->CustomGeneral.superbankerConfig), sizeof(m_superbankerConfig));

	//占位
	CopyMemory(&m_occupyseatConfig, &(pCustomConfig->CustomGeneral.occupyseatConfig), sizeof(m_occupyseatConfig));

	//时间
	m_nFreeTime = pCustomConfig->CustomGeneral.lFreeTime;
	m_nPlaceJettonTime = pCustomConfig->CustomGeneral.lBetTime;
	m_nGameEndTime = pCustomConfig->CustomGeneral.lEndTime;
	if(m_nFreeTime < TIME_FREE	|| m_nFreeTime > 99) m_nFreeTime = TIME_FREE;
	if(m_nPlaceJettonTime < TIME_PLACE_JETTON || m_nPlaceJettonTime > 99) m_nPlaceJettonTime = TIME_PLACE_JETTON;
	if(m_nGameEndTime < TIME_GAME_END || m_nGameEndTime > 99) m_nGameEndTime = TIME_GAME_END;

	//下注
	m_lAreaLimitScore = pCustomConfig->CustomGeneral.lAreaLimitScore;
	m_lUserLimitScore = pCustomConfig->CustomGeneral.lUserLimitScore;
	m_nEndGameMul = pCustomConfig->CustomGeneral.lEndGameMul;
	if(m_nEndGameMul < 1 || m_nEndGameMul > 100 ) m_nEndGameMul = 80;

	//库存
	m_lStorageStart = pCustomConfig->CustomGeneral.StorageStart;
	m_lStorageCurrent = m_lStorageStart;
	m_nStorageDeduct = pCustomConfig->CustomGeneral.StorageDeduct;
	m_lStorageMax1 = pCustomConfig->CustomGeneral.StorageMax1;
	m_lStorageMul1 = pCustomConfig->CustomGeneral.StorageMul1;
	m_lStorageMax2 = pCustomConfig->CustomGeneral.StorageMax2;
	m_lStorageMul2 = pCustomConfig->CustomGeneral.StorageMul2;
	if(m_lStorageMul1 < 0 || m_lStorageMul1 > 100 ) m_lStorageMul1 = 50;
	if(m_lStorageMul2 < 0 || m_lStorageMul2 > 100 ) m_lStorageMul2 = 80;
	
	//机器人
	m_nRobotListMaxCount = pCustomConfig->CustomAndroid.lRobotListMaxCount;

	LONGLONG lRobotBetMinCount = pCustomConfig->CustomAndroid.lRobotBetMinCount;
	LONGLONG lRobotBetMaxCount = pCustomConfig->CustomAndroid.lRobotBetMaxCount;
	m_nMaxChipRobot = rand()%(lRobotBetMaxCount-lRobotBetMinCount+1) + lRobotBetMinCount;
	if (m_nMaxChipRobot < 0)	m_nMaxChipRobot = 8;
	m_lRobotAreaLimit = pCustomConfig->CustomAndroid.lRobotAreaLimit;
	
	return;
}

// 库存优化
VOID CTableFrameSink::StorageOptimize()
{
	bool blRealPlayerWin = true;
	if (((m_lStorageCurrent > m_lStorageMax1) && (rand()%100 < m_lStorageMul1))
		|| ((m_lStorageCurrent > m_lStorageMax2) && (rand()%100 < m_lStorageMul2))
		|| rand() % 100 < 30)
	{
		blRealPlayerWin = true;
	}
	else
	{
		blRealPlayerWin = false;
	}

	LONGLONG lCirculateCount = 0L;

	while(true)
	{
		//真人玩家得分
		LONGLONG lRealPlayerWinScore = CalSysOrAndroidBankerScore();

		if (lCirculateCount > 10000)
		{
			//blRealPlayerWin = false;
			
			if (m_lStorageCurrent - lRealPlayerWinScore < 0 /*&& blRealPlayerWin == true*/)
			{
				blRealPlayerWin = false;
				lCirculateCount = 0;
				DispatchTableCard();
				continue;
			}

			break;
		}

		//库存不能为负数
		if (m_lStorageCurrent - lRealPlayerWinScore  <= 0)
		{
			DispatchTableCard();
			lCirculateCount++;
			continue;
		}

		//检验最大赔付比例
		LONGLONG lStorageCurrent = m_lStorageCurrent - lRealPlayerWinScore;
		double llCompensateRadio = (double)((double)30 / (double)100);
		if (m_lStorageCurrent * (1 - llCompensateRadio) >= lStorageCurrent)
		{
			DispatchTableCard();
			lCirculateCount++;
			continue;
		}

		//真人玩家输钱
		if (lRealPlayerWinScore <= 0 && blRealPlayerWin == false)
		{
			break;
		}
		
		if (lRealPlayerWinScore >= 0 && blRealPlayerWin == true)
		{
			break;
		}
		
		DispatchTableCard();
		lCirculateCount++;
	}
	
	//庄家赢牌
	if (m_lStorageCurrent - CalSysOrAndroidBankerScore()<0)
	{
		CString strFileName;
		strFileName.Format(TEXT("百人牛牛[%s]下注 信息.log"), m_pGameServiceOption->szServerName);

		WriteInfo(strFileName, TEXT("庄家赢牌"));

		m_pServerContro->GetBankerWinResult(m_cbTableCardArray, m_cbTableCard, m_lAllJettonScore);
	}
}

//计算系统及机器人坐庄时真人玩家得分
LONGLONG CTableFrameSink::CalSysOrAndroidBankerScore()
{
	ASSERT ((m_wCurrentBanker == INVALID_CHAIR && m_bEnableSysBanker == true)
		|| (m_wCurrentBanker != INVALID_CHAIR && m_pITableFrame->GetTableUserItem(m_wCurrentBanker)->IsAndroidUser() == true));

	//比较倍数
	bool bXianWin[4] = { false, false, false, false };							//比较输赢
	BYTE bMultiple[4] = { 1, 1, 1, 1 };											//比较倍数
	for (int i = 0; i < 4; i++)
	{
		bXianWin[i] = (m_GameLogic.CompareCard(m_cbTableCardArray[0], 5,m_cbTableCardArray[i+1], 5, bMultiple[i]) == 1);
	}

	//系统及机器人坐庄时真人玩家得分
	LONGLONG lRealPlayerWinScore = 0L;

	// 计算系统输赢
	for (WORD wChairID = 0; wChairID < GAME_PLAYER; wChairID++)
	{
		// 获取玩家
		IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(wChairID);

		// 过滤庄家
		if (wChairID == m_wCurrentBanker || pIServerUserItem == NULL)
		{
			continue;
		}

		//过滤机器人
		if (pIServerUserItem->IsAndroidUser() == true)
		{
			continue;
		}

		// 计算玩家下注
		for (int nAarea = 0; nAarea < 4; nAarea++)
		{
			if ( m_lUserJettonScore[nAarea+1][wChairID] != 0 )
			{
				if( bXianWin[nAarea] )
				{				
					lRealPlayerWinScore += m_lUserJettonScore[nAarea+1][wChairID] * bMultiple[nAarea];
				}
				else
				{
					lRealPlayerWinScore -= m_lUserJettonScore[nAarea+1][wChairID] * bMultiple[nAarea];
				}
			}
		}
	}

	return lRealPlayerWinScore;
}

//计算真人玩家坐庄时真人的得分
LONGLONG CTableFrameSink::CalRealPlayerBankerScore()
{
	ASSERT (m_wCurrentBanker != INVALID_CHAIR && m_pITableFrame->GetTableUserItem(m_wCurrentBanker)->IsAndroidUser() == false);

	//比较倍数
	bool bXianWin[4] = { false, false, false, false };							//比较输赢
	BYTE bMultiple[4] = { 1, 1, 1, 1 };											//比较倍数
	for (int i = 0; i < 4; i++)
	{
		bXianWin[i] = (m_GameLogic.CompareCard(m_cbTableCardArray[0], 5,m_cbTableCardArray[i+1], 5, bMultiple[i]) == 1);
	}

	LONGLONG lAndroidWinScore = 0L;

	// 计算系统输赢
	for (WORD wChairID = 0; wChairID < GAME_PLAYER; wChairID++)
	{
		// 获取玩家
		IServerUserItem *pIServerUserItem = m_pITableFrame->GetTableUserItem(wChairID);

		// 过滤庄家
		if (wChairID == m_wCurrentBanker || pIServerUserItem == NULL)
		{
			continue;
		}

		// 计算玩家下注
		for (int nAarea = 0; nAarea < 4; nAarea++)
		{
			if ( m_lUserJettonScore[nAarea+1][wChairID] != 0 )
			{
				if( bXianWin[nAarea] )
				{				
					if (pIServerUserItem->IsAndroidUser() == true)
					{
						lAndroidWinScore += m_lUserJettonScore[nAarea+1][wChairID] * bMultiple[nAarea];
					}
				}
				else
				{
					if (pIServerUserItem->IsAndroidUser() == true)
					{
						lAndroidWinScore -= m_lUserJettonScore[nAarea+1][wChairID] * bMultiple[nAarea];
					}
				}
			}
		}
	}
	
	return -lAndroidWinScore;
}

//查询是否扣服务费
bool CTableFrameSink::QueryBuckleServiceCharge(WORD wChairID)
{
	for (WORD i=0;i<GAME_PLAYER;i++)
	{
		IServerUserItem *pUserItem=m_pITableFrame->GetTableUserItem(i);
		if(pUserItem==NULL) continue;
		if(wChairID==i)
		{
			//返回下注
			for (int nAreaIndex=0; nAreaIndex<=AREA_COUNT; ++nAreaIndex) 
			{

				if (m_lUserJettonScore[nAreaIndex][wChairID] != 0)
				{
					return true;
				}
			}
			break;
		}
	}
	if (wChairID==m_wCurrentBanker)
	{
		return true;
	}

	//离线庄家
	if (wChairID == m_wOfflineBanker)
	{
		return true;
	}

	return false;
}

//是否衰减
bool CTableFrameSink::NeedDeductStorage()
{

	for ( int i = 0; i < GAME_PLAYER; ++i )
	{
		IServerUserItem *pIServerUserItem=m_pITableFrame->GetTableUserItem(i);
		if (pIServerUserItem == NULL ) continue; 

		if(!pIServerUserItem->IsAndroidUser())
		{
			for (int nAreaIndex=0; nAreaIndex<=AREA_COUNT; ++nAreaIndex) 
			{
				if (m_lUserJettonScore[nAreaIndex][i]!=0)
				{
					return true;
				}				
			}			
		}
	}

	return false;

}

//发送下注信息
void CTableFrameSink::SendUserBetInfo( IServerUserItem *pIServerUserItem )
{
	if(NULL == pIServerUserItem) return;

	//权限判断
	if(!CUserRight::IsGameCheatUser(pIServerUserItem->GetUserRight())) return;

	//构造数据
	CMD_S_SendUserBetInfo SendUserBetInfo;
	ZeroMemory(&SendUserBetInfo, sizeof(SendUserBetInfo));
	
	CopyMemory(&SendUserBetInfo.lUserStartScore, m_lUserStartScore, sizeof(m_lUserStartScore));
	CopyMemory(&SendUserBetInfo.lUserJettonScore, m_lUserJettonScore, sizeof(m_lUserJettonScore));

	//发送消息	
	m_pITableFrame->SendUserItemData(pIServerUserItem, SUB_S_SEND_USER_BET_INFO, &SendUserBetInfo, sizeof(SendUserBetInfo));
	
	return;
}

//测试写信息
void CTableFrameSink::WriteInfo( LPCTSTR pszFileName, LPCTSTR pszString )
{
	//设置语言区域
	char* old_locale = _strdup( setlocale(LC_CTYPE,NULL) );
	setlocale( LC_CTYPE, "chs" );

	CStdioFile myFile;
	CString strFileName;
	strFileName.Format(TEXT("%s"), pszFileName);
	BOOL bOpen = myFile.Open(strFileName, CFile::modeReadWrite|CFile::modeCreate|CFile::modeNoTruncate);
	if ( bOpen )
	{	
		myFile.SeekToEnd();
		myFile.WriteString( pszString );
		myFile.Flush();
		myFile.Close();
	}

	//还原区域设定
	setlocale( LC_CTYPE, old_locale );
	free( old_locale );
}

//获取会员等级
int CTableFrameSink::GetMemberOrderIndex(VIPINDEX vipIndex)
{
	ASSERT (vipIndex != VIP_INVALID);

	switch(vipIndex)
	{
	case VIP1_INDEX:
		{
			return 1;
		}
	case VIP2_INDEX:
		{
			return 2;
		}
	case VIP3_INDEX:
		{
			return 3;
		}
	case VIP4_INDEX:
		{
			return 4;
		}
	case VIP5_INDEX:
		{
			return 5;
		}
	default:
		return -1;
	}

	return -1;
}
//////////////////////////////////////////////////////////////////////////////////
