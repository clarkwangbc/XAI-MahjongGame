//
// Created by farmer on 2018/7/4.
//

#ifndef COCOSTUDIO_MAHJONG_GAMELAYER_H
#define COCOSTUDIO_MAHJONG_GAMELAYER_H

#include "IDialog.h"
#include "UIHelper.h"
#include "IPlayer.h"
#include "GameCmd.h"
#include "Utility.h"
#include "GameEngine.h"
#include "GameLogic.h"
#include "GameOverDlg.h"

class GameLayer : public IDialog<Layer>, IGameEngineEventListener {
    friend class GameOverDlg;
private:
    GameEngine *m_GameEngine;           //游戏引擎
    Node *m_FaceFrame[GAME_PLAYER];     //头像信息节点
    Node *m_PlayerPanel[GAME_PLAYER];   //玩家牌的区域
    Node *m_pOperateNotifyGroup;        //操作组
    Node* m_pOutCard;                   //正在出牌的节点
    ui::Text *m_pTextCardNum;           //剩余牌数量
    uint8_t m_MeChairID;                //自己的位置
    uint8_t m_iOutCardTimeOut;          //操作时间
//游戏变量
private:
    tagWeaveItem m_WeaveItemArray[GAME_PLAYER][MAX_WEAVE];                 //组合
    uint8_t m_cbCardIndex[GAME_PLAYER][MAX_INDEX];                         //玩家牌
    uint8_t m_cbWeaveItemCount[GAME_PLAYER];                               //组合数目
    uint8_t m_cbDiscardCount[GAME_PLAYER];                                 //丢弃数目
    uint8_t m_cbDiscardCard[GAME_PLAYER][MAX_DISCARD];                     //丢弃记录
    uint8_t m_cbLeftCardCount;                                             //剩余
    uint8_t m_cbBankerChair;                                               //庄
    bool m_bOperate;                                                       //是否可操作
    bool m_bMove;                                                          //是否移动牌
    Vec2 m_startVec = Vec2(0,0);                                           //记录位置
    float m_outY = 30;                                                     //偏移
    float m_cardPosY = 0.0f;                                               //偏移

public:
    GameLayer();    // 构造函数
    virtual ~GameLayer();   //析构
    CREATE_FUNC(GameLayer);

protected:
    Button* m_btnExit;
    Button* m_btnSetting;
    virtual const char* csbName() const {return "res/GameLayer.csb";}
    virtual void onUILoaded();

public:
    void initGame();                    //初始化游戏变量
    void sendCardTimerUpdate(float f);  //倒计时
    void onCardTouch(Ref *ref, ui::Widget::TouchEventType eventType);   //触摸牌的事件

private:
    virtual bool onUserEnterEvent(IPlayer *pIPlayer);            //玩家进入事件
    virtual bool onGameStartEvent(CMD_S_GameStart GameStart);    //游戏开始事件
    virtual bool onSendCardEvent(CMD_S_SendCard SendCard);       //发牌事件
    virtual bool onOutCardEvent(CMD_S_OutCard OutCard);          //出牌事件
    virtual bool onOperateNotifyEvent(CMD_S_OperateNotify OperateNotify);   //操作通知事件
    virtual bool onOperateResultEvent(CMD_S_OperateResult OperateResult);   //操作结果事件
    virtual bool onGameEndEvent(CMD_S_GameEnd& GameEnd);                     //移除特效

private:
    bool showAndUpdateHandCard();                                                   //显示手上的牌
    bool showAndUpdateDiscardCard();                                                //显示桌上的牌
    bool showSendCard(CMD_S_SendCard SendCard);                                     //发牌显示
    bool showOperateNotify(CMD_S_OperateNotify OperateNotify);                      //显示操作通知
    bool showAndPlayOperateEffect(uint8_t cbViewID,uint8_t cbOperateCode, bool bZm);//播放特效
    bool showTingResult(const uint8_t cbCardIndex[MAX_INDEX], tagWeaveItem WeaveItem[], uint8_t cbWeaveCount);   //显示听牌的结果
    bool showAndUpdateUserScore(int64_t lGameScoreTable[GAME_PLAYER]);             //更新分数
    
private:
    void removeEffectNode(std::string strNodeName); //移除特效
};


#endif //COCOSTUDIO_MAHJONG_GAMELAYER_H
