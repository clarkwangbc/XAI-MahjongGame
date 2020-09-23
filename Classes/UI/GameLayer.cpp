//
// Created by farmer on 2018/7/4.
// Modified by Clark on 2020/7/24
//

#include "GameLayer.h"
#include "AIPlayer.h"
#include "RealPlayer.h"
#include "AIEngine.h"
#include "SettingDlg.h"
#include "ConfidenceDlg.h"
#include "GameConfig.h"
#include "AlertDlg.h"
#include "DialogManager.h"
#include <fstream>

using namespace std;

static uint8_t USER_OP_MAX_TIME = 99; // 出牌倒计时秒数

GameLayer::GameLayer()  : IDialog() {
    RegDialogCtrl("Button_Exit", m_btnExit);
    RegDialogCtrl("Button_Set", m_btnSetting);
    RegDialogCtrl("OperateNotifyGroup", m_pOperateNotifyGroup);
    RegDialogCtrl("Text_LeftCard", m_pTextCardNum);
    for (int i = 0; i < GAME_PLAYER; i++) {
        //初始化头像节点数组
        RegDialogCtrl(utility::toString("face_frame_", i), m_FaceFrame[i]);
        RegDialogCtrl(utility::toString("PlayerPanel_", i), m_PlayerPanel[i]);
    }
    m_GameEngine = GameEngine::GetGameEngine();  //构造游戏引擎
    m_bOperate = false;
    m_bMove = false;
    m_pOperateNotifyGroup = NULL;
    m_pTextCardNum = NULL;
    m_pOutCard = NULL;
    m_iOutCardTimeOut = USER_OP_MAX_TIME;
    m_MeChairID = 0;
    initGame();
}

GameLayer::~GameLayer() {
}

void GameLayer::onUILoaded() {
    m_btnExit->addClickEventListener([this](Ref* sender) {
        // 退出游戏按钮
        auto alert = AlertDlg::create();
        alert->setAlertType(AlertDlg::ENUM_CONFIRM);
        alert->setCallback([]() {
            Director::getInstance()->end();
#if (CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
            exit(0);
#endif
        }, []() {
            DialogManager::shared()->closeAllDialog();
        });
        alert->setText("退出游戏后，本局游戏将直接结束无法恢复，确定是否退出？");
        DialogManager::shared()->showDialog(alert);
    });
    m_btnSetting->addClickEventListener([this](Ref* sender) {
        // 游戏设置按钮
        DialogManager::shared()->showDialog(SettingDlg::create());
    });
    // 玩家加入游戏
    RealPlayer *pIPlayer = new RealPlayer(IPlayer::MALE, this);
    m_GameEngine->onUserEnter(pIPlayer);
    m_MeChairID = pIPlayer->getChairID();
    // 创建个定时任务，用来添加机器人
    schedule([this](float) {
        //机器人玩家加入游戏，返回false说明已经满了，随机生成性别
        if (!m_GameEngine->onUserEnter(new AIPlayer(time(NULL) % 2 == 0 ? IPlayer::MALE : IPlayer::FEMALE, new AIEngine))) {
            unschedule("AiEnterGame");//人满，关闭定时任务
        };
    }, 1.0f, "AiEnterGame");
}

/**
 * 初始化游戏变量
 */
void GameLayer::initGame() {
    m_cbLeftCardCount = 0;
    m_cbBankerChair = INVALID_CHAIR;
    memset(&m_cbWeaveItemCount, 0, sizeof(m_cbWeaveItemCount));
    memset(&m_cbDiscardCount, 0, sizeof(m_cbDiscardCount));
    for (uint8_t i = 0; i < GAME_PLAYER; i++) {
        memset(m_cbCardIndex[i], 0, sizeof(m_cbCardIndex[i]));
        memset(m_WeaveItemArray[i], 0, sizeof(m_WeaveItemArray[i]));
        memset(m_cbDiscardCard[i], 0, sizeof(m_cbDiscardCard[i]));
    }
}

bool GameLayer::onUserEnterEvent(IPlayer *pIPlayer) {
    if (m_GameEngine->getPlayerCount() >= GAME_PLAYER) {
        unschedule("AiEnterGame");   //人满，关闭ai加入任务
    }
    if (pIPlayer && pIPlayer->getChairID() < GAME_PLAYER) {
        uint8_t uChairID = pIPlayer->getChairID();
        // 显示头像
        ui::ImageView *pImageHeader = dynamic_cast<ui::ImageView *>(UIHelper::seekNodeByName(m_FaceFrame[uChairID], "Image_Header"));  //头像
        ui::Text *pTextScore = dynamic_cast<ui::Text *>(UIHelper::seekNodeByName(m_FaceFrame[uChairID], "Text_Score"));  //头像
        pTextScore->setString("0");     //进来分数为0
        pImageHeader->loadTexture(utility::toString("res/GameLayer/im_defaulthead_", pIPlayer->getSexAsInt(), ".png"));    //设置头像
    }
    return true;
}

bool GameLayer::onGameStartEvent(CMD_S_GameStart GameStart) {
    cocos2d::log("接收到游戏开始事件");   //接收到游戏开始事件
    initGame();
    //调整头像位置
    m_FaceFrame[0]->runAction(MoveTo::create(0.50f, Vec2(0080.00f, 250.00f)));
    m_FaceFrame[1]->runAction(MoveTo::create(0.50f, Vec2(0080.00f, 380.00f)));
    m_FaceFrame[2]->runAction(MoveTo::create(0.50f, Vec2(1060.00f, 640.00f)));
    m_FaceFrame[3]->runAction(MoveTo::create(0.50f, Vec2(1200.00f, 380.00f)));
    //剩余的牌
    m_cbLeftCardCount = GameStart.cbLeftCardCount;
    m_cbBankerChair = GameStart.cbBankerUser;
    GameLogic::switchToCardIndex(GameStart.cbCardData, MAX_COUNT - 1, m_cbCardIndex[m_MeChairID]);
    //界面显示
    m_pTextCardNum->setString(utility::toString((int) m_cbLeftCardCount));
    showAndUpdateHandCard();
    showAndUpdateDiscardCard();
    return true;
}

/**
 * 发牌事件
 * @param SendCard
 * @return
 */
bool GameLayer::onSendCardEvent(CMD_S_SendCard SendCard) {
    m_iOutCardTimeOut = USER_OP_MAX_TIME; //重置计时器
    m_cbLeftCardCount--;
    if (SendCard.cbCurrentUser == m_MeChairID) {
        m_cbCardIndex[m_MeChairID][GameLogic::switchToCardIndex(SendCard.cbCardData)]++;

		//保存遗址
    }
    return showSendCard(SendCard);
}

/**
 * 出牌事件
 * @param OutCard
 * @return
 */
bool GameLayer::onOutCardEvent(CMD_S_OutCard OutCard) {
    if (OutCard.cbOutCardUser == m_MeChairID) {
        m_cbCardIndex[m_MeChairID][GameLogic::switchToCardIndex(OutCard.cbOutCardData)]--;
    }
    m_cbDiscardCard[OutCard.cbOutCardUser][m_cbDiscardCount[OutCard.cbOutCardUser]++] = OutCard.cbOutCardData;
    uint8_t cbViewID = m_GameEngine->switchViewChairID(OutCard.cbOutCardUser, m_MeChairID);
    cocostudio::timeline::ActionTimeline *action = CSLoader::createTimeline("res/SignAnim.csb");
    action->gotoFrameAndPlay(0, 10, true);
    Node *pSignNode = CSLoader::createNode("res/SignAnim.csb");
    pSignNode->setName("SignAnim");
    std::vector<Node *> aChildren;
    aChildren.clear();
    UIHelper::getChildren(m_rootNode, "SignAnim", aChildren);
    for (auto &subWidget : aChildren) {
        subWidget->removeFromParent();
    }
    ui::ImageView *pRecvCard = dynamic_cast<ui::ImageView *>(UIHelper::seekNodeByName(m_PlayerPanel[cbViewID], utility::toString("RecvCard_", (int) cbViewID)));
    if (pRecvCard) {
        pRecvCard->setVisible(false);
    }
    switch (cbViewID) {
        case 0: {
			DialogManager::shared()->showDialog(ConfidenceDlg::create());
			//用户出牌记录
			ofstream fCardLog;
			std::string filename = GameConfig::getInstance()->m_ParticipantId + "_cardlog.txt";
			std::string log_path = "C:/Users/clark/MahjongGame/UserLogs/" + filename;
			fCardLog.open(log_path, ios::app);
			fCardLog << ((OutCard.cbOutCardData & MASK_COLOR) >> 4) * 9 + (OutCard.cbOutCardData & MASK_VALUE) << endl;
			fCardLog.close();
            showAndUpdateHandCard();                     //更新手上的牌
            ui::Layout *pRecvCardList = dynamic_cast<ui::Layout *>(UIHelper::seekNodeByName(m_PlayerPanel[cbViewID], utility::toString("RecvHandCard_0")));
            pRecvCardList->removeAllChildren(); //移除出牌位置的牌
            ui::Layout *pDiscardCard0 = dynamic_cast<ui::Layout *>(this->getChildByName( "DiscardCard_0" ));//显示出的牌
            pDiscardCard0->removeAllChildren();
            uint8_t bDiscardCount = m_cbDiscardCount[OutCard.cbOutCardUser]; //12
            float x = 0;
            float y = 0;
            for (int i = 0; i < bDiscardCount; i++) {
                uint8_t col = static_cast<uint8_t>(i % 12);
                uint8_t row = static_cast<uint8_t>(i / 12);
                x = (col == 0) ? 0 : x;  //X复位
                y = row * 90;
                ui::ImageView *pCard0 = UIHelper::createDiscardCardImageView(cbViewID, m_cbDiscardCard[OutCard.cbOutCardUser][i]);
                pCard0->setAnchorPoint(Vec2(0, 0));
                pCard0->setPosition(Vec2(x, y));
                pCard0->setLocalZOrder(10 - row);
                x += 76;
                if (i == bDiscardCount - 1) {   //最后一张，添加动画效果
                    pSignNode->setAnchorPoint(Vec2(0.5, 0.5));
                    pSignNode->setPosition(Vec2(39, 110));
                    pCard0->addChild(pSignNode);
                    pSignNode->runAction(action);
                }
                pDiscardCard0->addChild(pCard0);
            }
            pDiscardCard0->setScale(0.7f, 0.7f);
            //听牌判断
            uint8_t cbWeaveItemCount = m_cbWeaveItemCount[OutCard.cbOutCardUser];
            tagWeaveItem *pTagWeaveItem = m_WeaveItemArray[OutCard.cbOutCardUser];
            uint8_t *cbCardIndex = m_cbCardIndex[OutCard.cbOutCardUser];
            showTingResult(cbCardIndex, pTagWeaveItem, cbWeaveItemCount);
            break;
        }
        case 1: {
            //显示出的牌
            ui::Layout *pDiscardCard1 = dynamic_cast<ui::Layout *>(this->getChildByName( "DiscardCard_1" ));
            pDiscardCard1->removeAllChildren();
            uint8_t bDiscardCount = m_cbDiscardCount[OutCard.cbOutCardUser]; //
            float x = 0;
            float y = 0;
            for (int i = 0; i < bDiscardCount; i++) {
                uint8_t col = static_cast<uint8_t>(i % 11);
                uint8_t row = static_cast<uint8_t>(i / 11);
                y = (col == 0) ? 0 : y;  //X复位
                x = 116 * row;
                ui::ImageView *pCard1 = UIHelper::createDiscardCardImageView(cbViewID, m_cbDiscardCard[OutCard.cbOutCardUser][i]);
                pCard1->setAnchorPoint(Vec2(0, 0));
                pCard1->setPosition(Vec2(x, 740 - y));
                pCard1->setLocalZOrder(col);
                y += 74;
                if (i == bDiscardCount - 1) {   //最后一张，添加动画效果
                    pSignNode->setAnchorPoint(Vec2(0.5, 0.5));
                    pSignNode->setPosition(Vec2(81, 110));
                    pCard1->addChild(pSignNode);
                    pSignNode->runAction(action);
                }
                pDiscardCard1->addChild(pCard1);
            }
            pDiscardCard1->setScale(0.5, 0.5);
            break;
        }
        case 2: {
            ui::Layout *pDiscardCard2 = dynamic_cast<ui::Layout *>(this->getChildByName( "DiscardCard_2" ));
            pDiscardCard2->removeAllChildren();
            uint8_t bDiscardCount = m_cbDiscardCount[OutCard.cbOutCardUser]; //12
            float x = 0;
            float y = 0;
            for (int i = 0; i < bDiscardCount; i++) {
                uint8_t col = static_cast<uint8_t>(i % 12);
                uint8_t row = static_cast<uint8_t>(i / 12);
                x = (col == 0) ? 0 : x;  //X复位
                y = 90 - row * 90;
                ui::ImageView *pCard2 = UIHelper::createDiscardCardImageView(cbViewID, m_cbDiscardCard[OutCard.cbOutCardUser][i]);
                pCard2->setAnchorPoint(Vec2(0, 0));
                pCard2->setPosition(Vec2(x, y));
                pCard2->setLocalZOrder(row);
                x += 76;
                if (i == bDiscardCount - 1) {   //最后一张，添加动画效果
                    pSignNode->setAnchorPoint(Vec2(0.5, 0.5));
                    pSignNode->setPosition(Vec2(39, 59));
                    pCard2->addChild(pSignNode);
                    pSignNode->runAction(action);
                }
                pDiscardCard2->addChild(pCard2);
            }
            pDiscardCard2->setScale(0.7f, 0.7f);

            break;
        }
        case 3: {
            //显示出的牌
            ui::Layout *pDiscardCard3 = dynamic_cast<ui::Layout *>(this->getChildByName( "DiscardCard_3" ));
            pDiscardCard3->removeAllChildren();
            uint8_t bDiscardCount = m_cbDiscardCount[OutCard.cbOutCardUser]; //
            float x = 0;
            float y = 0;
            for (int i = 0; i < bDiscardCount; i++) {
                uint8_t col = static_cast<uint8_t>(i % 11);
                uint8_t row = static_cast<uint8_t>(i / 11);
                y = (col == 0) ? 0 : y;  //X复位
                x = 240 - (116 * row);
                ui::ImageView *pCard3 = UIHelper::createDiscardCardImageView(cbViewID, m_cbDiscardCard[OutCard.cbOutCardUser][i]);
                pCard3->setAnchorPoint(Vec2(0, 0));
                pCard3->setPosition(Vec2(x, y));
                pCard3->setLocalZOrder(20 - col);
                y += 74;
                if (i == bDiscardCount - 1) {   //最后一张，添加动画效果
                    pSignNode->setAnchorPoint(Vec2(0.5, 0.5));
                    pSignNode->setPosition(Vec2(39, 110));
                    pCard3->addChild(pSignNode);
                    pSignNode->runAction(action);
                }
                pDiscardCard3->addChild(pCard3);
            }
            pDiscardCard3->setScale(0.5, 0.5);
            break;
        }
        default:
            break;
    }
    for (int j = 0; j < GAME_PLAYER; ++j) {   //发牌后隐藏导航
        ui::ImageView *pHighlight = dynamic_cast<ui::ImageView *>(this->getChildByName( utility::toString("Image_Wheel_", j) ));
        pHighlight->setVisible(false);
    }
    UIHelper::playSound(utility::toString("raw/Mahjong/"
                                , m_GameEngine->getPlayer(OutCard.cbOutCardUser)->getSexAsStr()
                                , "/mjt"
                                , utility::toString(((OutCard.cbOutCardData & MASK_COLOR) >> 4) + 1, "_", OutCard.cbOutCardData & MASK_VALUE)
                                , ".mp3"));    //播放牌的声音
    return true;
}


/**
 * 操作提示
 * @param OperateNotify
 * @return
 */
bool GameLayer::onOperateNotifyEvent(CMD_S_OperateNotify OperateNotify) {
    return showOperateNotify(OperateNotify);
}

/**
 * 操作结果事件
 * @param OperateResult
 * @return
 */
bool GameLayer::onOperateResultEvent(CMD_S_OperateResult OperateResult) {
    //更新数据
    tagWeaveItem weaveItem;
    memset(&weaveItem, 0, sizeof(tagWeaveItem));
    switch (OperateResult.cbOperateCode) {
        case WIK_NULL: {
            break;
        }
        case WIK_P: {
            weaveItem.cbWeaveKind = WIK_P;
            weaveItem.cbCenterCard = OperateResult.cbOperateCard;
            weaveItem.cbPublicCard = TRUE;
            weaveItem.cbProvideUser = OperateResult.cbProvideUser;
            weaveItem.cbValid = TRUE;
            m_WeaveItemArray[OperateResult.cbOperateUser][m_cbWeaveItemCount[OperateResult.cbOperateUser]++] = weaveItem;
            if (OperateResult.cbOperateUser == m_MeChairID) {  //自己
                uint8_t cbReomveCard[] = {OperateResult.cbOperateCard, OperateResult.cbOperateCard};
                GameLogic::removeCard(m_cbCardIndex[OperateResult.cbOperateUser], cbReomveCard, sizeof(cbReomveCard));
            }
            break;
        }
        case WIK_G: {
            weaveItem.cbWeaveKind = WIK_G;
            weaveItem.cbCenterCard = OperateResult.cbOperateCard;
            uint8_t cbPublicCard = (OperateResult.cbOperateUser == OperateResult.cbProvideUser) ? FALSE : TRUE;
            int j = -1;
            for (int i = 0; i < m_cbWeaveItemCount[OperateResult.cbOperateUser]; i++) {
                tagWeaveItem tempWeaveItem = m_WeaveItemArray[OperateResult.cbOperateUser][i];
                if (tempWeaveItem.cbCenterCard == OperateResult.cbOperateCard) {   //之前已经存在
                    cbPublicCard = TRUE;
                    j = i;
                }
            }
            weaveItem.cbPublicCard = cbPublicCard;
            weaveItem.cbProvideUser = OperateResult.cbProvideUser;
            weaveItem.cbValid = TRUE;
            if (j == -1) {
                m_WeaveItemArray[OperateResult.cbOperateUser][m_cbWeaveItemCount[OperateResult.cbOperateUser]++] = weaveItem;
            } else {
                m_WeaveItemArray[OperateResult.cbOperateUser][j] = weaveItem;
            }
            if (OperateResult.cbOperateUser == m_MeChairID) {  //自己
                GameLogic::removeAllCard(m_cbCardIndex[OperateResult.cbOperateUser], OperateResult.cbOperateCard);
            }
            break;
        }
        case WIK_H: {
            break;
        }
        default:
            break;
    }

    //更新界面
    uint8_t cbViewID = m_GameEngine->switchViewChairID(OperateResult.cbOperateUser, m_MeChairID);
    switch (cbViewID) {
        case 0: {   //自己操作反馈
            m_pOperateNotifyGroup->removeAllChildren();
            m_pOperateNotifyGroup->setVisible(false);
            ui::ImageView *pHighlight = dynamic_cast<ui::ImageView *>(this->getChildByName( "Image_Wheel_0" ));
            pHighlight->setVisible(true);
            break;
        }
        default:
            break;
    }
    std::string strSex = m_GameEngine->getPlayer(OperateResult.cbOperateUser)->getSexAsStr();
    switch (OperateResult.cbOperateCode) {
        case WIK_NULL: {
            break;
        }
        case WIK_P: {
            UIHelper::playSound(utility::toString("raw/Mahjong/", strSex, "/peng.mp3"));
            if (cbViewID == 0) {                    //如果是自己，碰完需要出牌
                uint8_t bTempCardData[MAX_COUNT];   //手上的牌
                memset(bTempCardData, 0, sizeof(bTempCardData));
                GameLogic::switchToCardData(m_cbCardIndex[OperateResult.cbOperateUser], bTempCardData, MAX_COUNT);
                uint8_t cbWeaveItemCount = m_cbWeaveItemCount[OperateResult.cbOperateUser]; //组合数量
                CMD_S_SendCard SendCard;
                memset(&SendCard, 0, sizeof(CMD_S_SendCard));
                SendCard.cbCurrentUser = OperateResult.cbOperateUser;
                SendCard.cbCardData = bTempCardData[MAX_COUNT - (cbWeaveItemCount * 3) - 1];

                showSendCard(SendCard);    //模拟发送一张牌

            }
            //移除桌上的哪张牌
            m_cbDiscardCount[OperateResult.cbProvideUser]--;  //移除桌上的牌
            showAndUpdateDiscardCard();
            break;
        }
        case WIK_G: {
            UIHelper::playSound(utility::toString("raw/Mahjong/", strSex, "/gang.mp3"));
            if (OperateResult.cbProvideUser != OperateResult.cbOperateUser) {     //放的杠
                m_cbDiscardCount[OperateResult.cbProvideUser]--;  //移除桌上的牌
                showAndUpdateDiscardCard();
            } else {                                                            //移除出牌位置的牌
                ui::Layout *pRecvCardList = dynamic_cast<ui::Layout *>(UIHelper::seekNodeByName(m_PlayerPanel[0], utility::toString("RecvHandCard_0")));
                pRecvCardList->removeAllChildren();
            }
            break;
        }
        case WIK_H: { //胡牌动作放在游戏结束 信息中
            if (OperateResult.cbOperateUser == OperateResult.cbProvideUser) {  //自摸
                UIHelper::playSound(utility::toString("raw/Mahjong/", strSex, "/zimo.mp3"));
            } else {
                UIHelper::playSound(utility::toString("raw/Mahjong/", strSex, "/hu.mp3"));
            }
            break;
        }
        default:
            break;
    }
    showAndPlayOperateEffect(cbViewID, OperateResult.cbOperateCode, OperateResult.cbProvideUser == OperateResult.cbOperateUser);
    showAndUpdateHandCard();
    return true;
}

/**
 * 游戏结束事件
 * @param GameEnd
 * @return
 */
bool GameLayer::onGameEndEvent(CMD_S_GameEnd& GameEnd) {
    showAndUpdateUserScore(GameEnd.lGameScoreTable);    //更新分数
    for (uint8_t i = 0; i < GAME_PLAYER; i++) {                    //播放音效
        if (FvMask::HasAny(GameEnd.cbHuUser, _MASK_(i))) {
            std::string strSex = m_GameEngine->getPlayer(i)->getSexAsStr();
            uint8_t cbViewID = m_GameEngine->switchViewChairID(i, m_MeChairID);
            if (FvMask::HasAny(GameEnd.cbHuUser, _MASK_(GameEnd.cbProvideUser))) {  //自摸
                UIHelper::playSound(utility::toString("raw/Mahjong/", strSex, "/zimo.mp3"));
                showAndPlayOperateEffect(cbViewID, WIK_H, true); //播放自摸动画
            } else {
                UIHelper::playSound(utility::toString("raw/Mahjong/", strSex, "/hu.mp3"));
                showAndPlayOperateEffect(cbViewID, WIK_H, false); //播放自摸动画
            }
        }
    }
    Node *pTingNode = UIHelper::seekNodeByName(m_PlayerPanel[0], "ting_0");    //隐藏听牌
    pTingNode->setVisible(false);
    this->removeEffectNode("EffectNode");
    this->m_pOperateNotifyGroup->removeAllChildren();
    this->m_pOperateNotifyGroup->setVisible(false);
    //显示结算界面
    this->runAction(Sequence::createWithTwoActions(DelayTime::create(0.1f), CallFuncN::create([this, GameEnd](Node*) {
        auto ui = GameOverDlg::create();
        ui->setGameUI(this, GameEnd);
        DialogManager::shared()->showDialog(ui);
        const auto &aChildren = this->m_pOperateNotifyGroup->getChildren();
        log("m_pOperateNotifyGroup.getChildren=%d", (int)aChildren.size());
    })));
    return true;
}

void GameLayer::sendCardTimerUpdate(float) {
    m_iOutCardTimeOut = static_cast<uint8_t>((m_iOutCardTimeOut-- < 1) ? 0 : m_iOutCardTimeOut);
    ui::ImageView *pTimer1 = dynamic_cast<ui::ImageView *>(this->getChildByName( "Image_Timer_1" ));
    ui::ImageView *pTimer0 = dynamic_cast<ui::ImageView *>(this->getChildByName( "Image_Timer_0" ));
    int t = m_iOutCardTimeOut / 10;   //十位
    int g = m_iOutCardTimeOut % 10;   //个位
    pTimer1->loadTexture(utility::toString("res/GameLayer/timer_", g, ".png"));
    pTimer0->loadTexture(utility::toString("res/GameLayer/timer_", t, ".png"));
}


/**
 * 发牌显示
 * @param SendCard
 * @return
 */
bool GameLayer::showSendCard(CMD_S_SendCard SendCard) {
    m_pOperateNotifyGroup->removeAllChildren();
    m_pOperateNotifyGroup->setVisible(true);
    this->unschedule(schedule_selector(GameLayer::sendCardTimerUpdate));
    this->schedule(schedule_selector(GameLayer::sendCardTimerUpdate), 1.0f);    //出牌计时
    sendCardTimerUpdate(1.0f);//立即执行
    m_pTextCardNum->setString(utility::toString((int) m_cbLeftCardCount));
    uint8_t cbViewID = m_GameEngine->switchViewChairID(SendCard.cbCurrentUser, m_MeChairID);
    m_bOperate = false;
    switch (cbViewID) {
        case 0: {
            m_bOperate = true;   //允许选牌
            ui::Layout *pRecvCardList = dynamic_cast<ui::Layout *>(UIHelper::seekNodeByName(m_PlayerPanel[cbViewID], utility::toString("RecvHandCard_0")));
            pRecvCardList->removeAllChildren();
            ui::ImageView *pCard = UIHelper::createHandCardImageView(cbViewID, SendCard.cbCardData);
            pCard->setAnchorPoint(Vec2(0, 0));
            pCard->setPosition(Vec2(0, 0));
            pCard->setTouchEnabled(true);
            pCard->setTag(SendCard.cbCardData);
            pCard->setName(utility::toString("bt_card_", (int) SendCard.cbCardData));
            pCard->addTouchEventListener(CC_CALLBACK_2(GameLayer::onCardTouch, this));
            pRecvCardList->addChild(pCard);
            UIHelper::seekNodeByName(m_PlayerPanel[cbViewID], "ting_0")->setVisible(false); //拿到牌，隐藏听牌界面
            if (SendCard.cbActionMask != WIK_NULL) {//发的牌存在动作，模拟发送操作通知
                CMD_S_OperateNotify OperateNotify;
                memset(&OperateNotify, 0, sizeof(CMD_S_OperateNotify));
                OperateNotify.cbActionMask = SendCard.cbActionMask;
                OperateNotify.cbActionCard = SendCard.cbCardData;
                OperateNotify.cbGangCount = SendCard.cbGangCount;
                memcpy(OperateNotify.cbGangCard, SendCard.cbGangCard, sizeof(OperateNotify.cbGangCard));
                showOperateNotify(OperateNotify);
            }
            break;
        }
        case 1:
        case 2:
        case 3:
        default:
            m_bOperate = false;  //不允许操作
            break;
    }
    for (int i = 0; i < GAME_PLAYER; i++) {
        ui::ImageView *pRecvCard = dynamic_cast<ui::ImageView *>(UIHelper::seekNodeByName(m_PlayerPanel[cbViewID], utility::toString("RecvCard_", i)));
        if (pRecvCard) {
            pRecvCard->setVisible(cbViewID == i);
        }
        ui::ImageView *pHighlight = dynamic_cast<ui::ImageView *>(this->getChildByName( utility::toString("Image_Wheel_", i) ));
        if (pHighlight) {
            pHighlight->setVisible(cbViewID == i);
        }
    }
    return true;
}


bool GameLayer::showTingResult(const uint8_t *cbCardIndex, tagWeaveItem *WeaveItem, uint8_t cbWeaveCount) {
    tagTingResult tingResult;
    memset(&tingResult, 0, sizeof(tagTingResult));
    Node *pTingNode = UIHelper::seekNodeByName(m_PlayerPanel[m_MeChairID], "ting_0");
    if (GameLogic::analyseTingCardResult(cbCardIndex, WeaveItem, cbWeaveCount, tingResult)) {  //听牌
        pTingNode->setVisible(true);
        Node *pTingCard = UIHelper::seekNodeByName(m_PlayerPanel[m_MeChairID], "ting_card");
        pTingCard->removeAllChildren();
        for (int i = 0; i < tingResult.cbTingCount; i++) {  //循环听的牌
            ui::ImageView *pTingCardImage = UIHelper::createDiscardCardImageView(0, tingResult.cbTingCard[i]);
            pTingCardImage->setAnchorPoint(Vec2(0, 0));
            pTingCardImage->setPosition(Vec2(76 * i + ((76.0f * 3) - (76.0f * tingResult.cbTingCount)) / 2.0f, 0));
            pTingCard->addChild(pTingCardImage);
        }
    } else {
        pTingNode->setVisible(false);
    }
    return true;
}

bool GameLayer::showAndUpdateUserScore(int64_t lGameScoreTable[GAME_PLAYER]) {
    for (uint8_t i = 0; i < GAME_PLAYER; i++) {
        uint8_t cbViewID = m_GameEngine->switchViewChairID(i, m_MeChairID);
        ui::Text *pScore = dynamic_cast<ui::Text *>(UIHelper::seekNodeByName(m_FaceFrame[cbViewID], "Gold_Label"));
        if (pScore) {
            pScore->setString(utility::toString(lGameScoreTable[i]));
        }
    }
    return true;
}

/**
 * 展示操作层
 * @param OperateNotify
 * @return
 */
bool GameLayer::showOperateNotify(CMD_S_OperateNotify OperateNotify) {
    if (OperateNotify.cbActionMask == WIK_NULL) {
        return true; //无动作
    }
    m_pOperateNotifyGroup->setVisible(true);
    m_pOperateNotifyGroup->removeAllChildren();
    auto loadUI = [this](const char* resPath, float x, float y, uint8_t tag) {
        Node *pNode = CSLoader::createNode(resPath);
        pNode->setPosition(Vec2(x, y));
        m_pOperateNotifyGroup->addChild(pNode);
        ui::Button *pBtn = dynamic_cast<ui::Button *>(pNode->getChildren().at(0));
        pBtn->setTag(tag);
        pBtn->addTouchEventListener([this, pBtn](Ref *ref, ui::Widget::TouchEventType eventType) {
            ui::Button *pRef = dynamic_cast<ui::Button *>(ref);
            if (pRef != NULL && pRef == pBtn) {
                std::string btnName = pRef->getName();
                int iTag = pRef->getTag();
                if (eventType == ui::Widget::TouchEventType::ENDED) {
                    CMD_C_OperateCard OperateCard;
                    memset(&OperateCard, 0, sizeof(CMD_C_OperateCard));
                    OperateCard.cbOperateCard = static_cast<uint8_t>(iTag);
                    OperateCard.cbOperateUser = m_MeChairID;
                    if (btnName == "btn_hu") {
                        OperateCard.cbOperateCode = WIK_H;
                    } else if (btnName == "btn_gang") {
                        OperateCard.cbOperateCode = WIK_G;
                    } else if (btnName == "btn_peng") {
                        OperateCard.cbOperateCode = WIK_P;
                    } else if (btnName == "btn_guo") {
                        OperateCard.cbOperateCode = WIK_NULL;
                    }
                    m_GameEngine->onUserOperateCard(OperateCard);
                    m_pOperateNotifyGroup->removeAllChildren();
                    m_pOperateNotifyGroup->setVisible(false);
                }
            }
        });
        return pBtn;
    };
    float x = 500.0f;
    float y = 65.0f;
    if ((OperateNotify.cbActionMask & WIK_H) == WIK_H) {
        loadUI("res/BtnHu.csb", x, y, OperateNotify.cbActionCard);
        x -= 160.0f;
    }
    if ((OperateNotify.cbActionMask & WIK_G) == WIK_G) {
        for (int i = 0; i < OperateNotify.cbGangCount; i++) {
            ui::Button *pGangBtn = loadUI("res/BtnGang.csb", x, y + (i * 120), OperateNotify.cbGangCard[i]);
            ui::ImageView *pImgGangCard = dynamic_cast<ui::ImageView *>(UIHelper::seekNodeByName(pGangBtn, "ImgGangCard"));
            pImgGangCard->loadTexture(UIHelper::getDiscardCardImagePath(0, OperateNotify.cbGangCard[i]));
            pImgGangCard->setVisible(OperateNotify.cbGangCount > 1);
            x -= 160.0f;
        }
    }
    if ((OperateNotify.cbActionMask & WIK_P) == WIK_P) {
        loadUI("res/BtnPeng.csb", x, y, OperateNotify.cbActionCard);
        x -= 160.0f;
    }
    loadUI("res/BtnGuo.csb", x, y, OperateNotify.cbActionCard);
    return true;
}

/**
 * 显示手上的牌
 * @return
 */
bool GameLayer::showAndUpdateHandCard() {
    for (uint8_t i = 0; i < m_GameEngine->getPlayerCount(); i++) {
        uint8_t viewChairID = m_GameEngine->switchViewChairID(i, m_MeChairID);
        uint8_t bCardData[MAX_COUNT];  //手上的牌
        memset(bCardData, 0, sizeof(bCardData));
        GameLogic::switchToCardData(m_cbCardIndex[i], bCardData, MAX_COUNT);
        uint8_t cbWeaveItemCount = m_cbWeaveItemCount[i]; //组合数量
        tagWeaveItem WeaveItemArray[MAX_WEAVE];         //组合
        memcpy(WeaveItemArray, m_WeaveItemArray[i], sizeof(WeaveItemArray));
        switch (viewChairID) {
            case 0: {
                ui::Layout *pHandCard0 = dynamic_cast<ui::Layout *>(UIHelper::seekNodeByName(m_PlayerPanel[viewChairID], "HandCard_0"));
                ui::Layout *pComb0 = dynamic_cast<ui::Layout *>(UIHelper::seekNodeByName(m_PlayerPanel[viewChairID], "comb_0"));  //组合
                pHandCard0->removeAllChildren();
                pComb0->removeAllChildren();
                int x = 0;  //宽起始位置
                for (uint8_t j = 0; j < cbWeaveItemCount; j++) {
                    tagWeaveItem weaveItem = WeaveItemArray[j];
                    Node *pWeaveNode = NULL;
                    if (weaveItem.cbWeaveKind == WIK_G) {
                        pWeaveNode = CSLoader::createNode("res/Gang0.csb");
                    }
                    if (weaveItem.cbWeaveKind == WIK_P) {
                        pWeaveNode = CSLoader::createNode("res/Peng0.csb");
                    }
                    assert(pWeaveNode != NULL);
                    pWeaveNode->setPosition(Vec2(x, 0));
                    const std::string &strImagePath = UIHelper::getDiscardCardImagePath(0, weaveItem.cbCenterCard);
                    const std::string &strBackImagePath = UIHelper::getBackCardImagePath(0, weaveItem.cbCenterCard);
                    ui::ImageView *pImageRight = dynamic_cast<ui::ImageView *>(UIHelper::seekNodeByName(pWeaveNode, "Image_right"));
                    ui::ImageView *pImageLeft = dynamic_cast<ui::ImageView *>(UIHelper::seekNodeByName(pWeaveNode, "Image_left"));
                    ui::ImageView *pImageCenter = dynamic_cast<ui::ImageView *>(UIHelper::seekNodeByName(pWeaveNode, "Image_center"));
                    pImageRight->loadTexture(strImagePath);
                    pImageCenter->loadTexture(strImagePath);
                    pImageLeft->loadTexture(strImagePath);
                    switch (m_GameEngine->switchViewChairID(weaveItem.cbProvideUser, m_MeChairID)) {
                        case 0: {
                            if (weaveItem.cbPublicCard == FALSE) {    //暗杠
                                pImageRight->loadTexture(strBackImagePath);
                                pImageCenter->loadTexture(strImagePath);
                                pImageLeft->loadTexture(strBackImagePath);
                            }
                            break;
                        }
                        case 1: {
                            pImageLeft->loadTexture(strBackImagePath);
                            break;
                        }
                        case 2: {
                            pImageCenter->loadTexture(strBackImagePath);
                            break;
                        }
                        case 3: {
                            pImageRight->loadTexture(strBackImagePath);
                            break;
                        }
                        default:
                            break;
                    }
                    pComb0->addChild(pWeaveNode);
                    x += 228;
                }
                for (uint8_t j = 0; j < MAX_COUNT - 1 - (3 * cbWeaveItemCount); j++) {
                    ui::ImageView *pCard = UIHelper::createHandCardImageView(viewChairID, bCardData[j]);
                    pCard->setAnchorPoint(Vec2(0, 0));
                    pCard->setPosition(Vec2(x, 0));
                    pCard->setTouchEnabled(true);
                    pCard->setTag(bCardData[j]);
                    pCard->setName(utility::toString("bt_card_", (int) bCardData[j]));
                    pCard->addTouchEventListener(CC_CALLBACK_2(GameLayer::onCardTouch, this));
                    pHandCard0->addChild(pCard);
                    x += 76;
                }
                break;
            }
            case 1: {
                ui::Layout *pHandCard1 = dynamic_cast<ui::Layout *>(UIHelper::seekNodeByName(m_PlayerPanel[viewChairID], "HandCard_1"));
                ui::Layout *pComb1 = dynamic_cast<ui::Layout *>(UIHelper::seekNodeByName(m_PlayerPanel[viewChairID], "comb_1"));  //组合
                pHandCard1->removeAllChildren();
                pComb1->removeAllChildren();
                int y = 800;  //高起始位置
                for (uint8_t j = 0; j < cbWeaveItemCount; j++) {
                    y -= 160;
                    tagWeaveItem weaveItem = WeaveItemArray[j];
                    Node *pWeaveNode = NULL;
                    if (weaveItem.cbWeaveKind == WIK_G) {
                        pWeaveNode = CSLoader::createNode("res/Gang1.csb");
                    }
                    if (weaveItem.cbWeaveKind == WIK_P) {
                        pWeaveNode = CSLoader::createNode("res/Peng1.csb");
                    }
                    assert(pWeaveNode != NULL);
                    pWeaveNode->setPosition(Vec2(0, y + 20));
                    const std::string &strImagePath = UIHelper::getDiscardCardImagePath(1, weaveItem.cbCenterCard);
                    const std::string &strBackImagePath = UIHelper::getBackCardImagePath(1, weaveItem.cbCenterCard);
                    ui::ImageView *pImageRight = dynamic_cast<ui::ImageView *>(UIHelper::seekNodeByName(pWeaveNode, "Image_right"));
                    ui::ImageView *pImageLeft = dynamic_cast<ui::ImageView *>(UIHelper::seekNodeByName(pWeaveNode, "Image_left"));
                    ui::ImageView *pImageCenter = dynamic_cast<ui::ImageView *>(UIHelper::seekNodeByName(pWeaveNode, "Image_center"));
                    pImageRight->loadTexture(strImagePath);
                    pImageCenter->loadTexture(strImagePath);
                    pImageLeft->loadTexture(strImagePath);
                    switch (m_GameEngine->switchViewChairID(weaveItem.cbProvideUser, m_MeChairID)) {
                        case 0: {
                            pImageRight->loadTexture(strBackImagePath);
                            break;
                        }
                        case 1: {
                            if (weaveItem.cbPublicCard == FALSE) {    //暗杠
                                pImageRight->loadTexture(strBackImagePath);
                                pImageCenter->loadTexture(strBackImagePath);
                                pImageLeft->loadTexture(strBackImagePath);
                            }
                            break;
                        }
                        case 2: {
                            pImageLeft->loadTexture(strBackImagePath);
                            break;
                        }
                        case 3: {
                            pImageCenter->loadTexture(strBackImagePath);
                            break;
                        }
                        default:
                            break;
                    }
                    pComb1->addChild(pWeaveNode);
                }
                for (uint8_t j = 0; j < MAX_COUNT - 1 - (3 * cbWeaveItemCount); j++) {
                    y -= 60;
                    ui::ImageView *pCard = UIHelper::createHandCardImageView(viewChairID, bCardData[j]);
                    pCard->setAnchorPoint(Vec2(0, 0));
                    pCard->setPosition(Vec2(0, y - 20));
                    pCard->setLocalZOrder(j);
                    pHandCard1->addChild(pCard);
                }
                break;
            }
            case 2: {
                ui::Layout *pHandCard2 = dynamic_cast<ui::Layout *>(UIHelper::seekNodeByName(m_PlayerPanel[viewChairID], "HandCard_2"));
                ui::Layout *pComb2 = dynamic_cast<ui::Layout *>(UIHelper::seekNodeByName(m_PlayerPanel[viewChairID], "comb_2"));  //组合
                pHandCard2->removeAllChildren();
                pComb2->removeAllChildren();
                int x = 1027;  //宽起始位置
                for (uint8_t j = 0; j < cbWeaveItemCount; j++) {
                    x -= 228;
                    tagWeaveItem weaveItem = WeaveItemArray[j];
                    Node *pWeaveNode = NULL;
                    if (weaveItem.cbWeaveKind == WIK_G) {
                        pWeaveNode = CSLoader::createNode("res/Gang0.csb");
                    }
                    if (weaveItem.cbWeaveKind == WIK_P) {
                        pWeaveNode = CSLoader::createNode("res/Peng0.csb");
                    }
                    assert(pWeaveNode != NULL);
                    pWeaveNode->setPosition(Vec2(x + 23, 0));
                    const std::string &strImagePath = UIHelper::getDiscardCardImagePath(2, weaveItem.cbCenterCard);
                    const std::string &strBackImagePath = UIHelper::getBackCardImagePath(2, weaveItem.cbCenterCard);
                    ui::ImageView *pImageRight = dynamic_cast<ui::ImageView *>(UIHelper::seekNodeByName(pWeaveNode, "Image_right"));
                    ui::ImageView *pImageLeft = dynamic_cast<ui::ImageView *>(UIHelper::seekNodeByName(pWeaveNode, "Image_left"));
                    ui::ImageView *pImageCenter = dynamic_cast<ui::ImageView *>(UIHelper::seekNodeByName(pWeaveNode, "Image_center"));
                    pImageRight->loadTexture(strImagePath);
                    pImageCenter->loadTexture(strImagePath);
                    pImageLeft->loadTexture(strImagePath);
                    switch (m_GameEngine->switchViewChairID(weaveItem.cbProvideUser, m_MeChairID)) {
                        case 0: {
                            pImageCenter->loadTexture(strBackImagePath);
                            break;
                        }
                        case 1: {
                            pImageLeft->loadTexture(strBackImagePath);
                            break;
                        }
                        case 2: {
                            if (weaveItem.cbPublicCard == FALSE) {    //暗杠
                                pImageRight->loadTexture(strBackImagePath);
                                pImageCenter->loadTexture(strBackImagePath);
                                pImageLeft->loadTexture(strBackImagePath);
                            }
                            break;
                        }
                        case 3: {
                            pImageRight->loadTexture(strBackImagePath);
                            break;
                        }
                        default:
                            break;
                    }
                    pComb2->addChild(pWeaveNode);
                }
                for (uint8_t j = 0; j < MAX_COUNT - 1 - (3 * cbWeaveItemCount); j++) {
                    x -= 76;
                    ui::ImageView *pCard = UIHelper::createHandCardImageView(viewChairID, bCardData[j]);
                    pCard->setAnchorPoint(Vec2(0, 0));
                    pCard->setPosition(Vec2(x, 0));
                    pHandCard2->addChild(pCard);
                }
                break;
            }
            case 3: {
                ui::Layout *pHandCard3 = dynamic_cast<ui::Layout *>(UIHelper::seekNodeByName(m_PlayerPanel[viewChairID], "HandCard_3"));
                ui::Layout *pComb3 = dynamic_cast<ui::Layout *>(UIHelper::seekNodeByName(m_PlayerPanel[viewChairID], "comb_3"));  //组合
                pHandCard3->removeAllChildren();
                pComb3->removeAllChildren();
                int y = 0;  //高起始位置
                for (uint8_t j = 0; j < cbWeaveItemCount; j++) {
                    tagWeaveItem weaveItem = WeaveItemArray[j];
                    Node *pWeaveNode = NULL;
                    if (weaveItem.cbWeaveKind == WIK_G) {
                        pWeaveNode = CSLoader::createNode("res/Gang1.csb");
                    }
                    if (weaveItem.cbWeaveKind == WIK_P) {
                        pWeaveNode = CSLoader::createNode("res/Peng1.csb");
                    }
                    assert(pWeaveNode != NULL);
                    pWeaveNode->setPosition(Vec2(0, y - 10));
                    const std::string &strImagePath = UIHelper::getDiscardCardImagePath(3, weaveItem.cbCenterCard);
                    const std::string &strBackImagePath = UIHelper::getBackCardImagePath(3, weaveItem.cbCenterCard);
                    ui::ImageView *pImageRight = dynamic_cast<ui::ImageView *>(UIHelper::seekNodeByName(pWeaveNode, "Image_right"));
                    ui::ImageView *pImageLeft = dynamic_cast<ui::ImageView *>(UIHelper::seekNodeByName(pWeaveNode, "Image_left"));
                    ui::ImageView *pImageCenter = dynamic_cast<ui::ImageView *>(UIHelper::seekNodeByName(pWeaveNode, "Image_center"));
                    pImageRight->loadTexture(strImagePath);
                    pImageCenter->loadTexture(strImagePath);
                    pImageLeft->loadTexture(strImagePath);
                    switch (m_GameEngine->switchViewChairID(weaveItem.cbProvideUser, m_MeChairID)) {
                        case 0: {
                            pImageRight->loadTexture(strBackImagePath);
                            break;
                        }
                        case 1: {
                            pImageCenter->loadTexture(strBackImagePath);
                            break;
                        }
                        case 2: {
                            pImageLeft->loadTexture(strBackImagePath);
                            break;
                        }
                        case 3: {
                            if (weaveItem.cbPublicCard == FALSE) {    //暗杠
                                pImageRight->loadTexture(strBackImagePath);
                                pImageCenter->loadTexture(strBackImagePath);
                                pImageLeft->loadTexture(strBackImagePath);
                            }
                            break;
                        }
                        default:
                            break;
                    }
                    pComb3->addChild(pWeaveNode);
                    y += 160;
                }
                for (uint8_t j = 0; j < MAX_COUNT - 1 - (3 * cbWeaveItemCount); j++) {
                    ui::ImageView *pCard = UIHelper::createHandCardImageView(viewChairID, bCardData[j]);
                    pCard->setAnchorPoint(Vec2(0, 0));
                    pCard->setPosition(Vec2(0, y - 20));
                    pCard->setLocalZOrder(MAX_COUNT - j);
                    pHandCard3->addChild(pCard);
                    y += 60;
                }
                break;
            }
            default:
                break;
        }
    }
    return true;
}

/**
 * 播放特效
 * @param cbViewID
 * @param cbOperateCode
 * @param bZm
 * @return
 */
bool GameLayer::showAndPlayOperateEffect(uint8_t cbViewID, uint8_t cbOperateCode, bool bZm) {
    std::string strEffect("");
    switch (cbOperateCode) {
        case WIK_NULL:
            break;
        case WIK_P: 
            strEffect = "res/EffectPeng.csb";
            break;
        case WIK_G: 
            strEffect = "res/EffectGang.csb";
            break;
        case WIK_H: 
            strEffect = (bZm ? "res/EffectZm.csb" : "res/EffectHu.csb");
            break;
        default:
            break;
    }
    if (strEffect != "") {
        Vec2 pos;
        switch (cbViewID) {
            case 0:    //自己操作反馈
                pos = { 640.0f , 200.0f };
                break;
            case 1: 
                pos = { 440.0f , 360.0f };
                break;
            case 2: 
                pos = { 640.0f , 520.0f };
                break;
            case 3: 
                pos = { 840.0f , 360.0f };
                break;
            default:
                break;
        }
        std::string strNodeName = "EffectNode";
        Node *pEffectNode = CSLoader::createNode(strEffect);
        pEffectNode->setPosition(pos);
        cocostudio::timeline::ActionTimeline *action = CSLoader::createTimeline(strEffect);
        action->gotoFrameAndPlay(0, false);
        pEffectNode->setName(strNodeName);
        m_rootNode->addChild(pEffectNode);
        if (cbOperateCode != WIK_H) {    //胡牌不自动删除动画
            action->setLastFrameCallFunc(CC_CALLBACK_0(GameLayer::removeEffectNode, this, strNodeName));
        }
        pEffectNode->runAction(action);
    }
    return true;
}

/**
 * 移除动画节点
 * @param strNodeName
 */
void GameLayer::removeEffectNode(std::string strNodeName) {
    std::vector<Node *> aChildren;
    UIHelper::getChildren(m_rootNode, strNodeName, aChildren);
    for (auto &subChild : aChildren) {
        subChild->removeFromParent();
    }
    aChildren.clear();
}

/**
 * 显示、更新桌上的牌
 * @return
 */
bool GameLayer::showAndUpdateDiscardCard() {
    for (uint8_t cbChairID = 0; cbChairID < m_GameEngine->getPlayerCount(); cbChairID++) {
        uint8_t cbViewID = m_GameEngine->switchViewChairID(cbChairID, m_MeChairID);
        switch (cbViewID) {
            case 0: {
                ui::Layout *pDiscardCard0 = dynamic_cast<ui::Layout *>(this->getChildByName( "DiscardCard_0" ));
                pDiscardCard0->removeAllChildren();
                uint8_t bDiscardCount = m_cbDiscardCount[cbChairID];
                float x = 0;
                float y = 0;
                for (uint8_t i = 0; i < bDiscardCount; i++) {
                    uint8_t col = static_cast<uint8_t>(i % 12);
                    uint8_t row = static_cast<uint8_t>(i / 12);
                    x = (col == 0) ? 0 : x;  //X复位
                    y = row * 90;
                    ui::ImageView *pCard0 = UIHelper::createDiscardCardImageView(cbViewID, m_cbDiscardCard[cbChairID][i]);
                    pCard0->setAnchorPoint(Vec2(0, 0));
                    pCard0->setPosition(Vec2(x, y));
                    pCard0->setLocalZOrder(10 - row);
                    x += 76;
                    pDiscardCard0->addChild(pCard0);
                }
                pDiscardCard0->setScale(0.7f, 0.7f);
                break;
            }
            case 1: {
                //显示出的牌
                ui::Layout *pDiscardCard1 = dynamic_cast<ui::Layout *>(this->getChildByName( "DiscardCard_1" ));
                pDiscardCard1->removeAllChildren();
                uint8_t bDiscardCount = m_cbDiscardCount[cbChairID];
                float x = 0;
                float y = 0;
                for (uint8_t i = 0; i < bDiscardCount; i++) {
                    uint8_t col = static_cast<uint8_t>(i % 11);
                    uint8_t row = static_cast<uint8_t>(i / 11);
                    y = (col == 0) ? 0 : y;  //X复位
                    x = 116 * row;
                    ui::ImageView *pCard1 = UIHelper::createDiscardCardImageView(cbViewID, m_cbDiscardCard[cbChairID][i]);
                    pCard1->setAnchorPoint(Vec2(0, 0));
                    pCard1->setPosition(Vec2(x, 740 - y));
                    pCard1->setLocalZOrder(col);
                    y += 74;
                    pDiscardCard1->addChild(pCard1);
                }
                pDiscardCard1->setScale(0.5, 0.5);
                break;
            }
            case 2: {
                ui::Layout *pDiscardCard2 = dynamic_cast<ui::Layout *>(this->getChildByName( "DiscardCard_2" ));
                pDiscardCard2->removeAllChildren();
                uint8_t bDiscardCount = m_cbDiscardCount[cbChairID];
                float x = 0;
                float y = 0;
                for (uint8_t i = 0; i < bDiscardCount; i++) {
                    uint8_t col = static_cast<uint8_t>(i % 12);
                    uint8_t row = static_cast<uint8_t>(i / 12);
                    x = (col == 0) ? 0 : x;  //X复位
                    y = 90 - row * 90;
                    ui::ImageView *pCard2 = UIHelper::createDiscardCardImageView(cbViewID, m_cbDiscardCard[cbChairID][i]);
                    pCard2->setAnchorPoint(Vec2(0, 0));
                    pCard2->setPosition(Vec2(x, y));
                    pCard2->setLocalZOrder(row);
                    x += 76;
                    pDiscardCard2->addChild(pCard2);
                }
                pDiscardCard2->setScale(0.7f, 0.7f);

                break;
            }
            case 3: {
                //显示出的牌
                ui::Layout *pDiscardCard3 = dynamic_cast<ui::Layout *>(this->getChildByName( "DiscardCard_3" ));
                pDiscardCard3->removeAllChildren();
                uint8_t bDiscardCount = m_cbDiscardCount[cbChairID];
                float x = 0;
                float y = 0;
                for (uint8_t i = 0; i < bDiscardCount; i++) {
                    uint8_t col = static_cast<uint8_t>(i % 11);
                    uint8_t row = static_cast<uint8_t>(i / 11);
                    y = (col == 0) ? 0 : y;  //X复位
                    x = 240 - (116 * row);
                    ui::ImageView *pCard3 = UIHelper::createDiscardCardImageView(cbViewID, m_cbDiscardCard[cbChairID][i]);
                    pCard3->setAnchorPoint(Vec2(0, 0));
                    pCard3->setPosition(Vec2(x, y));
                    pCard3->setLocalZOrder(20 - col);
                    y += 74;
                    pDiscardCard3->addChild(pCard3);
                }
                pDiscardCard3->setScale(0.5, 0.5);
                break;
            }
            default:
                break;
        }
    }
    return true;
}

/**
 * 出牌
 * @param ref
 * @param eventType
 */
void GameLayer::onCardTouch(Ref *ref, ui::Widget::TouchEventType eventType) {
    if (m_pOutCard != NULL && m_pOutCard != ref) return;
    if (m_bOperate) {
        ui::ImageView *pCard = dynamic_cast<ui::ImageView *>(ref);
        ui::Layout *pHandCard0 = dynamic_cast<ui::Layout *>(UIHelper::seekNodeByName(m_PlayerPanel[0], "HandCard_0"));
        const auto &aChildren = pHandCard0->getChildren();
        for (auto &subWidget : aChildren) {
            Node *child = dynamic_cast<Node *>(subWidget);
            if (pCard != child) {
                child->setPositionY(0.0f);
            }
        }
        ui::Layout *pRecvHandCard0 = dynamic_cast<ui::Layout *>(UIHelper::seekNodeByName(m_PlayerPanel[0], "RecvHandCard_0"));
        const auto &bChildren = pRecvHandCard0->getChildren();
        for (auto &subWidget : bChildren) {
            Node *child = dynamic_cast<Node *>(subWidget);
            if (pCard != child) {
                child->setPositionY(0.0f);
            }
        }
        if (pCard != NULL) {
            std::string btCardName = pCard->getName();
            switch (eventType) {
                case ui::Widget::TouchEventType::BEGAN: {
                    m_pOutCard = dynamic_cast<Node *>(ref);         //正在出牌状态
                    m_bMove = false;                                //是否移动牌状态
                    m_startVec = pCard->getPosition();              //记录开始位置
                    //听牌判断
                    uint8_t cbCardData = static_cast<uint8_t>(pCard->getTag());   //牌
                    uint8_t cbWeaveItemCount = m_cbWeaveItemCount[m_MeChairID];
                    tagWeaveItem *pTagWeaveItem = m_WeaveItemArray[m_MeChairID];
                    uint8_t cbTingCard[MAX_INDEX];
                    memset(&cbTingCard, 0, sizeof(cbTingCard));
                    uint8_t *cbCardIndex = m_cbCardIndex[m_MeChairID];
                    memcpy(&cbTingCard, cbCardIndex, sizeof(cbTingCard));   //内存拷贝
                    cbTingCard[GameLogic::switchToCardIndex(cbCardData)]--;   //移除要出的牌进行分析
                    showTingResult(cbTingCard, pTagWeaveItem, cbWeaveItemCount);
                    break;
                }
                case ui::Widget::TouchEventType::MOVED: {
                    Vec2 a = pCard->getTouchBeganPosition();
                    Vec2 b = pCard->getTouchMovePosition();
                    pCard->setPosition(m_startVec + (b - a));       //移动
                    if (b.y - a.y > 60 || abs(b.x - a.x) > 30) {    //移动判定
                        m_bMove = true;
                    }
                    break;
                }
                case ui::Widget::TouchEventType::CANCELED:
                case ui::Widget::TouchEventType::ENDED: {
                    m_pOutCard = NULL;                             //结束出牌状态
                    Vec2 endVec = pCard->getPosition();
                    if (endVec.y - m_startVec.y > 118) {
                        CCLOG("out card");
                        CMD_C_OutCard OutCard;
                        memset(&OutCard, 0, sizeof(CMD_C_OutCard));
                        OutCard.cbCardData = static_cast<uint8_t >(pCard->getTag());
                        m_GameEngine->onUserOutCard(OutCard);
                    } else {
                        if (m_startVec.y == m_cardPosY) {                               //正常状态
                            if (m_bMove) {
                                pCard->setPosition(m_startVec);                         //移动
                            } else {
                                pCard->setPosition(m_startVec + Vec2(0, m_outY));       //移动
                            }
                        } else if (m_startVec.y == m_cardPosY + m_outY) {               //选牌状态
                            Vec2 a = pCard->getTouchBeganPosition();                    //触摸上半部分，撤销出牌
                            if (a.y > 118) {
                                pCard->setPosition(m_startVec - Vec2(0, m_outY));
                            } else {                                                   //触摸下半部分，出牌
                                pCard->setPosition(m_startVec);
                                CMD_C_OutCard OutCard;
                                memset(&OutCard, 0, sizeof(CMD_C_OutCard));
                                OutCard.cbCardData = static_cast<uint8_t>(pCard->getTag());
                                m_GameEngine->onUserOutCard(OutCard);
                            }
                        } else {
                            pCard->setPosition(m_startVec);    //移动
                        }
                    }
                    break;
                }

            }
        }
    }
}


