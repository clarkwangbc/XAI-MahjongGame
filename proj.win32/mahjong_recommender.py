# -*- coding: UTF-8 -*-

import pandas as pd
import numpy as np
import paramiko

# 各种地址
robot_url = "169.254.111.214"
player_cards_path = "C:/Users/clark/MahjongGame/ResultLogs/PlayerLog.txt"
player_cards_peng_path = "C:/Users/clark/MahjongGame/ResultLogs/PlayerLog_Peng.txt"
player_cards_gang_path = "C:/Users/clark/MahjongGame/ResultLogs/PlayerLog_Gang.txt"
ai_card_path = "C:/Users/clark/MahjongGame/ResultLogs/AILog.txt"

result_suggestion_path = "C:/Users/clark/MahjongGame/ResultLogs/Suggestion.txt"
result_explanation_path = "C:/Users/clark/MahjongGame/ResultLogs/Explanation.txt"
robot_sug_path = "/home/nao/clark-XAI/Results_Sug.txt"
robot_exp_path = "/home/nao/clark-XAI/Results_Exp.txt"

# 分别载入三张概率表，分别对应普通牌（万，筒，条），风牌（东、西、南、北），和键牌（中、发、白）
tab_normal = pd.read_csv("mahjong_tables/mahjong_normal.csv")
tab_feng = pd.read_csv("mahjong_tables/mahjong_feng.csv")
tab_jian = pd.read_csv("mahjong_tables/mahjong_jian.csv")

int_2_pai = {
    1: "一筒", 2: "二筒", 3: "三筒", 4: "四筒", 5: "五筒", 6: "六筒", 7: "七筒", 8: "八筒", 9: "九筒",
    10: "一万", 11: "两万", 12: "三万", 13: "四万", 14: "五万", 15: "六万", 16: "七万", 17: "八万", 18: "九万",
    19: "一条", 20: "二条", 21: "三条", 22: "四条", 23: "五条", 24: "六条", 25: "七条", 26: "八条", 27: "九条",
    28: "东风", 29: "南风", 30: "西风", 31: "北风", 32: "红中", 33: "发财", 34: "白板"
}


def read_player_card(str_path):
    fo = open(str_path, 'r')
    fc = fo.readline().strip("\n")
    fc_tmp = fc.split(",")
    fc_tmp = list(filter(None, fc_tmp))
    player_card = []
    for i in range(len(fc_tmp)):
        card, num = fc_tmp[i].split(":")
        card = int(card) + 1
        num = int(num)
        for j in range(num):
            player_card.append(card)
    return player_card


###########
# 出牌算法 #
###########
# 这个函数是计算给定的牌的获胜概率
# 输入的参数：
#     input_cards: 输入牌 数组
def calc(input_cards):
    # 将原始的牌数组转化成一个稀疏的数组
    cards = np.zeros(34)
    for i in input_cards:
        cards[i - 1] = cards[i - 1] + 1

    # 下面计算各类牌的键值，便于后续查表
    wan_key = 0
    tong_key = 0
    tiao_key = 0
    feng_key = 0
    jian_key = 0

    for i in range(0, 9):
        wan_key = wan_key * 10 + cards[i]

    for i in range(9, 18):
        tong_key = tong_key * 10 + cards[i]

    for i in range(18, 27):
        tiao_key = tiao_key * 10 + cards[i]

    for i in range(27, 31):
        feng_key = feng_key * 10 + cards[i]

    for i in range(31, 34):
        jian_key = jian_key * 10 + cards[i]

    # 查表，得到各个键值对应的概率
    # 这里tmp要重新定义一下
    tmp = [tab_normal[tab_normal["pai"] == wan_key],
           tab_normal[tab_normal["pai"] == tong_key],
           tab_normal[tab_normal["pai"] == tiao_key],
           tab_feng[tab_feng["pai"] == feng_key],
           tab_jian[tab_jian["pai"] == jian_key]
           ]

    ret = []
    clac_table_info(ret, tmp, 0, 0, 0.0)

    return max(ret)


# 这是一个迭代函数，计算各种花色的牌作为将牌的获胜几率，将得到的结果返回值传入的参数 ret 中
# 输入参数：
#     ret : 传入的空数组，用于记录计算的结果
#     tmp : 各个花色的参数表查询结果
#     index : 二叉树的层数索引
#     jiang : 是否作为将牌
#     cur : 累计概率值
def clac_table_info(ret, tmp, index, jiang, cur):
    if index >= len(tmp):
        if jiang:
            ret.append(cur)
        return
    for row in tmp[index].index:
        r_j = tmp[index].loc[[row]].iat[0, 1]
        r_p = tmp[index].loc[[row]].iat[0, 2]
        if r_j == 0:
            clac_table_info(ret, tmp, index + 1, jiang, cur + r_p)
        else:
            clac_table_info(ret, tmp, index + 1, r_j, cur + r_p)


# 此函数计算最优出牌，先删除某一张牌，然后剩下牌的获胜概率：
# 输入参数：
#   str_path：用户的牌记录
def chu_pai(str_path):
    input_cards = read_player_card(str_path)
    result = []
    pai_array = []
    p_cache = np.zeros(34)
    for i in range(len(input_cards)):
        p_max = 0.0
        pai = input_cards[i]
        if p_cache[pai - 1] == 0:
            pai_array.append(pai)
            tmp = input_cards[:]
            del tmp[i]
            tmp_score = calc(tmp)
            if tmp_score > p_max:
                p_max = tmp_score
            p_cache[pai - 1] = 1
            result.append(float(format(p_max, ".4f")))
    return result, pai_array, input_cards


###########
# 避免出牌 #
###########
# 该函数将从文件读取的字符串转换程
# 字符串的形式如下"2:1,4:1,7:1,10:1,12:1,13:1,14:1,15:1,21:1,23:1,25:1,30:1,31:1"
# 其中冒号前的数字代表牌的序号，冒号后的数字代表该张牌的张数（1~4）
# 该函数返回一个字典，形式与字符串形式类似
def str_2_pai_dict(pai_str):
    tmp_str = pai_str.split(",")
    pai = {}
    for i in range(len(tmp_str)):
        if tmp_str[i] != "":
            card, num = tmp_str[i].split(":")
            card = int(card) + 1
            num = int(num)
            pai[card] = num
    return pai


# 该函数用于计算AI可以碰、或者吃、或者胡的牌
#   file_path: AI牌的路径
def avoid_pai(file_path):
    fo = open(file_path, 'r')
    fc = fo.readline().strip("$\n")
    ai_1, ai_2, ai_3 = fc.split("$")
    ai_1 = str_2_pai_dict(ai_1)
    ai_2 = str_2_pai_dict(ai_2)
    ai_3 = str_2_pai_dict(ai_3)

    #  防止碰牌
    result_peng = [k for k, v in ai_1.items() if v >= 2] + \
                  [k for k, v in ai_2.items() if v >= 2] + \
                  [k for k, v in ai_3.items() if v >= 2]
    result_peng = sorted(result_peng)

    return result_peng


#######
# 解释器
#######

# 考虑避免碰牌
# 对比解释：通过比较出牌后胜率最大的两张牌，选择其中较优的一个。
# 输出两个结果：
#   chu_pai：最后应该出的牌
#   explanation：出该张牌的解释
# 参数：
#   pai：用户当前的手牌
#   prob：每张手牌的出掉后的胜率
#   avoid：其他用户会碰的牌
def contrast_explain(origin_pai, pai, prob, avoid):
    # 首先需要挑选出最大的几个
    prob_np = np.array(prob)
    max_array = prob_np.argsort()[-4:][::-1]

    index = 1
    result_log = [0]

    if pai[max_array[0]] not in avoid:
        result_log[0] = 1
    else:
        contrast_compare(pai, prob, avoid, max_array, index, result_log)

    if len(result_log) == 1:
        discard_pai = int_2_pai[pai[max_array[0]]]
        suggestions = "我选择出" + discard_pai + "。"
        explanation = context_analysis_explain(origin_pai, pai[max_array[0]])
    elif result_log[-1] == -1:
        discard_pai = int_2_pai[pai[max_array[0]]]
        opt_pai_1 = ""
        for pai_index in max_array[1:-1]:
            opt_pai_1 = opt_pai_1 + "、" + int_2_pai[pai[pai_index]]
        opt_pai_2 = int_2_pai[pai[max_array[len(result_log) - 1]]]
        suggestions = "我认为出" + discard_pai + "。"
        explanation = "其实出" + discard_pai + opt_pai_1 + "都不错，但是都容易被人碰走，而" + discard_pai + \
                      "是获胜概率最高的一张。此外还可以考虑出" + opt_pai_2 + "这张牌。出" + opt_pai_2 + \
                      "虽然不容易被碰走，但是会让牌型变得比较差，因此还是不推荐。" + "所以还是出" + discard_pai + "最好。"

    else:
        discard_pai = int_2_pai[pai[max_array[len(result_log) - 1]]]
        opt_pai = int_2_pai[pai[max_array[0]]]
        suggestions = "出" + discard_pai + "。"
        explanation = "根据计算，" + opt_pai + \
                      "的获胜概率更高，但是也容易被人碰走。为了兼顾牌型和防守，我认为出" + discard_pai + "更好。"

    return discard_pai, suggestions, explanation


# Contrast Explain 的迭代函数，逻辑判断
def contrast_compare(pai, prob, avoid, compare, index, result_log):
    if index < len(compare):
        # 如果概率最大的那张牌在avoid集合中，则比较概率大小,如果两者的差值大于0.5，则还是选则概率最大的那张牌，并修改记录。
        if prob[compare[0]] - prob[compare[index]] > 0.5:
            result_log[0] = 1
            result_log.append(-1)
            return
        else:
            # 如果概率最大的那张牌和比较的牌都在avoid的合集中，则换一张比较的牌继续比较
            # 此时标记为-2，表示这前面这几张牌都在那个集合中
            if pai[compare[index]] in avoid:
                result_log.append(-2)
                contrast_compare(pai, prob, avoid, compare, index + 1, result_log)
            # 如果概率最大的那张牌在avoid的集合中，但是比较的那张牌不在，则选择出该张牌
            # 此时标记为1， 表示这张牌是符合要求的牌
            else:
                result_log.append(1)
                return
    else:
        return


# 该函数将根据具体的牌型进行分析，给出解释
# 输入参数：
#   origin_pai：数组，输入最初的牌组（即允许有重复的数字的牌组）
#   chu_pai: 整数，输入需要出的牌
def context_analysis_explain(origin_pai, discard_pai):
    if 27 < discard_pai < 35:
        explain_text = "因为这是箭牌，无法形成顺子。如果手上有单张的箭牌，建议打出去。"
    elif discard_pai in [1, 9, 10, 18, 19, 27]:
        explain_text = "因为一和九处于字牌的两端，能够形成顺子的机会远远小于处于中间的牌，因此建议打出去。"
    elif discard_pai < 9:
        tmp_pai = [i for i in origin_pai if i < 10]
        tmp_pai.remove(discard_pai)
        explain_text = "出掉这张牌后，可以形成：" + combo_analysis(tmp_pai) + "这样的组合，利于后续出牌"
    elif discard_pai < 18:
        tmp_pai = [i for i in origin_pai if 9 < i < 19]
        tmp_pai.remove(discard_pai)
        explain_text = "出掉这张牌后，可以形成：" + combo_analysis(tmp_pai) + "这样的组合，利于后续出牌"
    else:
        tmp_pai = [i for i in origin_pai if 18 < i < 28]
        tmp_pai.remove(discard_pai)
        explain_text = "出掉这张牌后，可以形成：" + combo_analysis(tmp_pai) + "这样的组合，利于后续出牌"
    return explain_text


def combo_analysis(pai):
    explain_text = ""
    potential_combo = 0
    combo = 0
    while pai:
        tmp_pai = pai[0]
        # 如果有2个以上的某张牌
        if pai.count(tmp_pai) >= 2:
            explain_text = explain_text + "，" + str(pai.count(tmp_pai)) + "个" + int_2_pai[tmp_pai]
            while tmp_pai in pai:
                pai.remove(tmp_pai)
        # 如果只有单张牌,检查后续两张是否有牌
        else:
            if (tmp_pai + 1 in pai) and (tmp_pai + 2 in pai):
                explain_text = explain_text + int_2_pai[tmp_pai] + int_2_pai[tmp_pai + 1] + \
                               int_2_pai[tmp_pai + 2] + "的顺子，"
                pai.remove(tmp_pai)
                pai.remove(tmp_pai + 1)
                pai.remove(tmp_pai + 2)
            elif (tmp_pai + 1 in pai) and (tmp_pai + 2 not in pai):
                explain_text = explain_text + int_2_pai[tmp_pai] + int_2_pai[tmp_pai + 1] + "的准顺子，"
                pai.remove(tmp_pai)
                pai.remove(tmp_pai + 1)
            elif (tmp_pai + 1 not in pai) and (tmp_pai + 2 in pai):
                explain_text = explain_text + int_2_pai[tmp_pai] + int_2_pai[tmp_pai + 2] + "的准顺子，"
                pai.remove(tmp_pai)
                pai.remove(tmp_pai + 2)
            else:
                explain_text = explain_text + "单张的" + int_2_pai[tmp_pai]
                pai.remove(tmp_pai)
    return explain_text


# 上传文件至机器人
def save_upload(suggestion, explanation):
    # 保存出牌建议
    fo_s = open(result_suggestion_path, "w")
    fo_s.write(suggestion)
    fo_s.close()
    # 保存出牌解释
    fo_e = open(result_explanation_path, "w")
    fo_e.write(explanation)
    fo_e.close()
    # 上传至机器人
    transport = paramiko.Transport(robot_url, 22)
    transport.connect(username="nao", password="hciuser524")
    sftp = paramiko.SFTPClient.from_transport(transport)
    sftp.put(result_suggestion_path, robot_sug_path)
    sftp.put(result_explanation_path, robot_exp_path)
    sftp.close()
    transport.close()
    return


# 测试用的
def test_print(suggestion, explanation):
    # 保存出牌建议
    fo_s = open(result_suggestion_path, "w")
    fo_s.write(suggestion)
    fo_s.close()
    # 保存出牌解释
    fo_e = open(result_explanation_path, "w")
    fo_e.write(explanation)
    fo_e.close()
    # 打印结果
    print(suggestion)
    print(explanation)
    return


##########
# 出牌方法 #
##########
# 最终的一般的出牌函数
def normal_discard():
    # 计算
    chu_pai_prob, player_simple_pai, player_origin_pai = chu_pai(player_cards_path)
    avoid_peng = avoid_pai(ai_card_path)
    chu, suggestion, explanation = contrast_explain(player_origin_pai, player_simple_pai, chu_pai_prob, avoid_peng)

    # test_print(suggestion, explanation)

    # 将结果上传至机器人
    save_upload(suggestion, explanation)
    return


# 判断是否碰牌
def peng_judge_discard():
    chu_pai_prob, player_simple_pai, player_origin_pai = chu_pai(player_cards_peng_path)
    prob_np = np.array(chu_pai_prob)
    max_array = prob_np.argsort()[-4:][::-1]
    discard_pai = player_simple_pai[max_array[0]]
    if discard_pai == player_origin_pai[-1]:
        suggestion = "建议不要碰！"
        explanation = "碰了后牌型太差了。"
    else:
        suggestion = "可以碰！"
        explanation = "碰了后出" + int_2_pai[discard_pai]

    # test_print(suggestion, explanation)

    # 将结果上传至机器人
    save_upload(suggestion, explanation)
    return


# 判断是否杠牌
def gang_judge():
    player_origin_pai = read_player_card(player_cards_gang_path)
    gang_card = player_origin_pai[-1]
    if player_origin_pai[-1] not in player_origin_pai[0:-1]:
        suggestion = "这里一定要杠。"
        explanation = "杠了后增加牌的番数"
    else:
        origin_score = calc(player_origin_pai[0:-1])
        while gang_card in player_origin_pai:
            player_origin_pai.remove(gang_card)
        gang_score = calc(player_origin_pai)
        if origin_score > gang_score:
            suggestion = "这里最好不要杠。"
            explanation = "因为杠了后会破坏牌型。"
        else:
            suggestion = "这里一定要杠。"
            explanation = "杠了后既增加牌的番数，又不怎么破坏现有的牌型。"

    # test_print(suggestion, explanation)

    # 将结果上传至机器人
    save_upload(suggestion, explanation)
    return
