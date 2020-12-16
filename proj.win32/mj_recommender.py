# -*- coding: UTF-8 -*-

import pandas as pd
import numpy as np
import json
import random
from naoqi import ALProxy

# 分别载入三张概率表，分别对应普通牌（万，筒，条），风牌（东、西、南、北），和键牌（中、发、白）
prob_normal = pd.read_csv("mahjong_tables/mahjong_normal.csv")
prob_feng = pd.read_csv("mahjong_tables/mahjong_feng.csv")
prob_jian = pd.read_csv("mahjong_tables/mahjong_jian.csv")
ting_normal = pd.read_csv("mahjong_tables/ting_normal.csv")

int_2_pai = {
    1: "一筒", 2: "二筒", 3: "三筒", 4: "四筒", 5: "五筒", 6: "六筒", 7: "七筒", 8: "八筒", 9: "九筒",
    10: "一万", 11: "两万", 12: "三万", 13: "四万", 14: "五万", 15: "六万", 16: "七万", 17: "八万", 18: "九万",
    19: "一条", 20: "二条", 21: "三条", 22: "四条", 23: "五条", 24: "六条", 25: "七条", 26: "八条", 27: "九条",
    28: "东风", 29: "南风", 30: "西风", 31: "北风", 32: "红中", 33: "发财", 34: "白板"
}

pai_2_int = {
    "一筒": 1, "二筒": 2, "三筒": 3, "四筒": 4, "五筒": 5, "六筒": 6, "七筒": 7, "八筒": 8, "九筒": 9,
    "一万": 10, "两万": 11, "三万": 12, "四万": 13, "五万": 14, "六万": 15, "七万": 16, "八万": 17, "九万": 18,
    "一条": 19, "二条": 20, "三条": 21, "四条": 22, "五条": 23, "六条": 24, "七条": 25, "八条": 26, "九条": 27,
    "东风": 28, "南风": 29, "西风": 30, "北风": 31, "红中": 32, "发财": 33, "白板": 34
}


###########
# 出牌算法 #
###########

# 读取玩家手牌
# 文件中保存的手牌数字是从0~33，这里需要转换成1~34
#       str_path: 文件读取路径
def read_player_card(str_path):
    fo = open(str_path, 'r')
    fc = fo.readline().strip("\n")
    fc_tmp = fc.split(",")
    fc_tmp = list(filter(None, fc_tmp))
    player_card = []
    for i in range(len(fc_tmp)):
        card, num = fc_tmp[i].split(":")
        card = int(card) + 1  # 注意这里要加1
        num = int(num)
        for j in range(num):
            player_card.append(card)
    fo.close()
    return player_card


# 该函数将牌按照花色分成几个小牌组
# 输入的参数：
#     input_cards: 输入牌 数组
def divide_cards(input_cards):
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
        tong_key = tong_key * 10 + cards[i]

    for i in range(9, 18):
        wan_key = wan_key * 10 + cards[i]

    for i in range(18, 27):
        tiao_key = tiao_key * 10 + cards[i]

    for i in range(27, 31):
        feng_key = feng_key * 10 + cards[i]

    for i in range(31, 34):
        jian_key = jian_key * 10 + cards[i]

    result = {"wan": wan_key,
              "tong": tong_key,
              "tiao": tiao_key,
              "feng": feng_key,
              "jian": jian_key}

    return result


# 这个函数是计算给定的牌的获胜概率
# 输入的参数：
#     input_cards: 输入牌 数组
def calc(input_cards):
    keys = divide_cards(input_cards)

    # 查表，得到各个键值对应的概率
    # 这里tmp要重新定义一下
    tmp = [prob_normal[prob_normal["pai"] == keys["wan"]],
           prob_normal[prob_normal["pai"] == keys["tong"]],
           prob_normal[prob_normal["pai"] == keys["tiao"]],
           prob_feng[prob_feng["pai"] == keys["feng"]],
           prob_jian[prob_jian["pai"] == keys["jian"]]
           ]

    ret = []
    calculate_table_info(ret, tmp, 0, 0, 0.0)

    return max(ret)


# 这是一个迭代函数，计算各种花色的牌作为将牌的获胜几率，将得到的结果返回值传入的参数 ret 中
# 输入参数：
#     ret : 传入的空数组，用于记录计算的结果
#     tmp : 各个花色的参数表查询结果
#     index : 二叉树的层数索引
#     jiang : 是否作为将牌
#     cur : 累计概率值
def calculate_table_info(ret, tmp, index, jiang, cur):
    if index >= len(tmp):
        if jiang:
            ret.append(cur)
        return
    for row in tmp[index].index:
        r_j = tmp[index].loc[[row]].iat[0, 1]
        r_p = tmp[index].loc[[row]].iat[0, 2]
        if r_j == 0:
            calculate_table_info(ret, tmp, index + 1, jiang, cur + r_p)
        else:
            calculate_table_info(ret, tmp, index + 1, r_j, cur + r_p)


# 此函数计算最优出牌，先删除某一张牌，然后剩下牌的获胜概率：
# 输入参数：
#   str_path：文件读取路径
#   返回一个字典
def calc_card_prob(str_path):
    input_cards = read_player_card(str_path)
    p_result = []
    card_array = []
    p_cache = np.zeros(34)
    for i in range(len(input_cards)):
        p_max = 0.0
        pai = input_cards[i]
        if p_cache[pai - 1] == 0:
            card_array.append(pai)
            tmp = input_cards[:]
            del tmp[i]
            tmp_score = calc(tmp)
            if tmp_score > p_max:
                p_max = tmp_score
            p_cache[pai - 1] = 1
            p_result.append(float(format(p_max, ".4f")))
    result = {"OriginalCard": input_cards,
              "SimplifiedCard": card_array,
              "Probability": p_result}
    return result


######################
# 计算牌组最需要的牌 #
######################
# 计算牌组最需要的牌
def need_card(input_cards):
    card_keys = divide_cards(input_cards)

    if card_keys["tong"] == 0:
        tong_need = {"pai": [], "num": -3}
    else:
        tong_need = need_card_single(card_keys["tong"])
    if card_keys["wan"] == 0:
        wan_need = {"pai": [], "num": -3}
    else:
        wan_need = need_card_single(card_keys["wan"])
        wan_need["pai"] = [k + 9 for k in wan_need["pai"]]
    if card_keys["tiao"] == 0:
        tiao_need = {"pai": [], "num": -3}
    else:
        tiao_need = need_card_single(card_keys["tiao"])
        tiao_need["pai"] = [k + 18 for k in tiao_need["pai"]]

    result = []
    if tong_need["num"] == 1:
        result.extend(tong_need["pai"])
    if wan_need["num"] == 1:
        result.extend(wan_need["pai"])
    if tiao_need["num"] == 1:
        result.extend(tiao_need["pai"])

    if result:
        return result
    else:
        if tong_need["num"] == 2:
            result.extend(tong_need["pai"])
        if wan_need["num"] == 2:
            result.extend(wan_need["pai"])
        if tiao_need["num"] == 2:
            result.extend(tiao_need["pai"])
        return result


# 计算单种花色需要的牌
def need_card_single(card_code):
    if card_code == 0:
        return {"num": 0, "pai": 0}
    else:
        table = ting_normal[(ting_normal["pai"] == card_code) & (ting_normal["cha_num"] < 2)]
        table_1 = table[(table["cha_num"] == 0) & (table["jiang"] == 0)]
        table_2 = table[(table["cha_num"] == 0) & (table["jiang"] == 1)]
        # table_3 = table[(table["cha_num"] == 1) & (table["jiang"] == 0)]
        # table_4 = table[(table["cha_num"] == 1) & (table["jiang"] == 1)]

        if len(table_1) != 0:
            return {"num": 1, "pai": decode_card(table_1.iat[0, 3])}
        elif len(table_2) != 0:
            return {"num": 1, "pai": decode_card(table_2.iat[0, 3])}
        # elif len(table_3) != 0:
        #     return {"num": 2, "pai": decode_card(table_3.iat[0, 3])}
        # elif len(table_4) != 0:
        #     return {"num": 2, "pai": decode_card(table_4.iat[0, 3])}
        else:
            return {"num": -2, "pai": []}  # 表示本牌在2张以内都无法听牌


def decode_card(card_code):
    result = []
    if card_code == -1:
        return result
    else:
        for i in range(9):
            tmp2 = card_code % 10
            if tmp2 == 1:
                result.append(9 - i)
            card_code = int(card_code / 10)
        return result


# 计算场上已经明牌的数量
# 输入文件的地址
# 返回一个34长度的稀疏数组，数组的每个元素代表该index对应的牌的已经出来的牌的数量。(序号未纠正，需要+1)
def left_cards(file_path1, file_path2):
    # 牌桌上已经明牌
    fo = open(file_path1, 'r')
    fc = fo.readline().strip("\n")
    card_array = []
    while fc:
        fc_tmp = fc.split(",")
        fc_tmp = list(filter(None, fc_tmp))
        card_array.extend(fc_tmp)
        fc = fo.readline().strip("\n")
    card_array = [int(k) for k in card_array]  # 这里不纠正序号是因为下面要充当数组序号，从0开始
    fo.close()

    # 自己的牌
    player_cards = read_player_card(file_path2)
    player_cards = [int(k) - 1 for k in player_cards]
    card_array.extend(player_cards)

    result = np.zeros(34)
    for i in card_array:
        result[i] = result[i] + 1
    result = 4 - result
    return result


# 读取玩家的听牌表
# 输入文件的地址
# 返回一个字典，字典包括的可以形成听牌的出牌，以及出牌后所听的牌(序号已纠正)
def player_ting(file_path):
    fo = open(file_path, "r")
    fc = fo.readline().strip("\n")
    ting_dict = {}
    while fc:
        fc_tmp1, fc_tmp2 = fc.split(":")
        if (fc_tmp2 != " ") & (fc_tmp2 != ""):
            fc_tmp2 = fc_tmp2.replace(" ", "")
            fc_tmp2 = fc_tmp2.strip(",")
            fc_tmp2 = fc_tmp2.split(",")
            fc_tmp2 = [int(k) + 1 for k in fc_tmp2]  # 纠正序号
            ting_dict[int(fc_tmp1) + 1] = fc_tmp2  # 纠正序号
        fc = fo.readline().strip("\n")
    fo.close()
    return ting_dict


# 从文件中读取可能点炮的牌
# 输入文件的地址
# 返回一个数组，数组中包括了可能点炮的牌的序号(序号已纠正)
def avoid_card(file_path):
    fo = open(file_path, "r")
    fc = fo.readline().strip("\n")
    avoid = []
    while fc:
        fc_tmp1, fc_tmp2 = fc.split(": ")
        if fc_tmp2:
            fc_tmp2 = fc_tmp2.replace(" ", "")
            fc_tmp2 = fc_tmp2.strip(",")
            fc_tmp2 = fc_tmp2.split(",")
            fc_tmp2 = [int(k) + 1 for k in fc_tmp2]
            avoid.extend(fc_tmp2)
        fc = fo.readline().strip("\n")
    fo.close()
    return avoid


# 计算备选牌组
def alt_card():
    player_ting_path = "C:/Users/clark/MahjongGame/XAIMethod-AutoLevel/ResultLogs/PlayerTingLog.txt"
    player_cards_path = "C:/Users/clark/MahjongGame/XAIMethod-AutoLevel/ResultLogs/PlayerLog.txt"
    discard_path = "C:/Users/clark/MahjongGame/XAIMethod-AutoLevel/ResultLogs/Discard.txt"
    ai_ting_path = "C:/Users/clark/MahjongGame/XAIMethod-AutoLevel/ResultLogs/TingResult.txt"

    player_ting_cards = player_ting(player_ting_path)
    card_p = calc_card_prob(player_cards_path)
    left_card_pool = left_cards(discard_path, player_cards_path)
    avoid_cards = avoid_card(ai_ting_path)

    # 若听牌表中存在的牌不存在于玩家手牌中，则将该牌删去
    for card in player_ting_cards.keys():
        if card not in card_p["SimplifiedCard"]:
            del player_ting_cards[card]

    # 检查用户是否听牌
    if player_ting_cards:
        # 判断听牌的数量
        if len(player_ting_cards.keys()) > 1:
            # 如果备选的出牌多于 2 张
            # 则选择两张听牌数量最多的作为备选牌
            max1 = 0
            index1 = player_ting_cards.keys()[0]
            max2 = 0
            index2 = player_ting_cards.keys()[0]
            for key in player_ting_cards:
                tmp = player_ting_cards[key]
                card_num = 0
                # 统计听牌的牌数
                for card in tmp:
                    card_num = card_num + left_card_pool[card - 1]
                if card_num > max1:
                    max2 = max1
                    index2 = index1
                    max1 = card_num
                    index1 = key
                elif card_num > max2:
                    max2 = card_num
                    index2 = key
            if index1 == index2:
                tmp = player_ting_cards.keys()[:]
                tmp.remove(index1)
                index2 = tmp[0]
                tmp = player_ting_cards[index2]
                max2 = 0
                for card in tmp:
                    max2 = max2 + left_card_pool[card - 1]

            alt_card_info = {index1: {"left_cards": player_ting_cards[index1],
                                      "left_cards_num": int(max1),
                                      "is_in_avoid": (index1 in avoid_cards),
                                      "is_ting": True,
                                      "p": card_p["Probability"][card_p["SimplifiedCard"].index(index1)],
                                      },
                             index2: {"left_cards": player_ting_cards[index2],
                                      "left_cards_num": int(max2),
                                      "is_in_avoid": (index2 in avoid_cards),
                                      "is_ting": True,
                                      "p": card_p["Probability"][card_p["SimplifiedCard"].index(index2)],
                                      },
                             }
            return alt_card_info
        else:
            # 如果只听一张牌
            # 则从其他牌中选一张作为备选牌
            alt_card_info = {}
            card_num = 0
            for key in player_ting_cards:  # 注意，这里player_ting_cards 这个列表里只有一个元素
                tmp = player_ting_cards[key]
                for card in tmp:
                    card_num = card_num + left_card_pool[card - 1]
                alt_card_info = {key: {"left_cards": player_ting_cards[key],
                                       "left_cards_num": int(card_num),
                                       "is_in_avoid": (key in avoid_cards),
                                       "is_ting": True,
                                       "p": card_p["Probability"][card_p["SimplifiedCard"].index(key)],
                                       }}
            # 从普通牌中寻找概率最高的一张
            # 首先要把可以听牌的这两张和点炮的牌剔除掉
            # 选概率最高的一张
            # 计算其需要的牌
            # 计算需要的牌的数量
            max_index = 0
            max_p = 0
            for i in range(len(card_p["SimplifiedCard"])):
                if card_p["SimplifiedCard"][i] not in avoid_cards:
                    if card_p["SimplifiedCard"][i] not in player_ting_cards.keys():
                        if max_p < card_p["Probability"][i]:
                            max_index = i
                            max_p = card_p["Probability"][i]
            key = card_p["SimplifiedCard"][max_index]
            tmp_cards = card_p["OriginalCard"][:]
            tmp_cards.remove(key)
            key_need_card = need_card(tmp_cards)
            key_need_num = 0
            for card in key_need_card:
                key_need_num = key_need_num + left_card_pool[card - 1]
            alt_card_info[key] = {"left_cards": key_need_card,
                                  "left_cards_num": int(key_need_num),
                                  "is_in_avoid": False,
                                  "is_ting": False,
                                  "p": max_p,
                                  }
            return alt_card_info
    else:
        # 如果没有可以听的牌，则选择概率最高的两张
        index1 = 0
        index2 = 0
        max_p1 = 0
        max_p2 = 0
        for i in range(len(card_p["SimplifiedCard"])):
            if card_p["SimplifiedCard"][i] not in avoid_cards:
                if max_p1 < card_p["Probability"][i]:
                    index2 = index1
                    max_p2 = max_p1
                    index1 = i
                    max_p1 = card_p["Probability"][i]
                elif max_p2 < card_p["Probability"][i]:
                    index2 = i
                    max_p2 = card_p["Probability"][i]

        key1 = card_p["SimplifiedCard"][index1]
        tmp_cards1 = card_p["OriginalCard"][:]
        tmp_cards1.remove(key1)
        key_need_card1 = need_card(tmp_cards1)
        key_need_num1 = 0
        for card in key_need_card1:
            key_need_num1 = key_need_num1 + left_card_pool[card - 1]

        key2 = card_p["SimplifiedCard"][index2]
        tmp_cards2 = card_p["OriginalCard"][:]
        tmp_cards2.remove(key2)
        key_need_card2 = need_card(tmp_cards2)
        key_need_num2 = 0
        for card in key_need_card2:
            key_need_num2 = key_need_num2 + left_card_pool[card - 1]

        alt_card_info = {key1: {"left_cards": key_need_card1,
                                "left_cards_num": int(key_need_num1),
                                "is_in_avoid": False,
                                "is_ting": False,
                                "p": max_p1, },
                         key2: {"left_cards": key_need_card2,
                                "left_cards_num": int(key_need_num2),
                                "is_in_avoid": False,
                                "is_ting": False,
                                "p": max_p2, }}

        return alt_card_info


# 将牌数组转化为一连串的牌名
def array_2_card(array):
    result = ""
    # array = array.sort()
    for card in array:
        result = result + int_2_pai[card] + "、"
    result = result.strip("、")
    return result


# 对比解释
def select_explain(info):
    suggestion = ""  # type: str
    deductive_explanation = ""  # type: str
    contrast_explanation = ""  # type: str
    out_card = -1

    key1 = info.keys()[0]
    key2 = info.keys()[1]
    if info[key1]["is_in_avoid"] > info[key2]["is_in_avoid"]:
        # 若key1点炮，key2不点炮
        suggestion = "推荐的牌有" + int_2_pai[key1] + "或" + int_2_pai[key2] + \
                     "。但是" + int_2_pai[key2] + "更好一点。"
        contrast_explanation = (contrast_explanation +
                                "下列的两张牌均是对获胜概率提升最大的出牌。但是" +
                                int_2_pai[key1] +
                                "很有可能点炮，因此不推荐。")
        out_card = key2

    elif info[key1]["is_in_avoid"] < info[key2]["is_in_avoid"]:
        # 若key2点炮，key1不点炮
        suggestion = "推荐" + int_2_pai[key1] + "或" + int_2_pai[key2] + \
                     "。但建议出" + int_2_pai[key1] + "。"
        contrast_explanation = (contrast_explanation +
                                "下列的两张牌均是对获胜概率提升最大的出牌。但是" +
                                int_2_pai[key2] +
                                "很有可能点炮，因此不推荐。")
        out_card = key1

    elif (info[key1]["is_in_avoid"] == info[key2]["is_in_avoid"]) & (not info[key1]["is_in_avoid"]):
        # 若key1和key2均不点炮（key1和key2均点炮的情况不太可能发生）
        if info[key1]["is_ting"] > info[key2]["is_ting"]:
            # key1听牌，key2不听牌
            suggestion = "可以考虑" + int_2_pai[key1] + "或" + int_2_pai[key2] + \
                         "。但我跟偏向" + int_2_pai[key1] + "。"
            contrast_explanation = (contrast_explanation +
                                    "下面两张牌都是根据概率计算推荐的出牌，但是出" +
                                    int_2_pai[key1] +
                                    "就听牌了，而出另外一张不能听牌。")
            out_card = key1

        elif info[key1]["is_ting"] < info[key2]["is_ting"]:
            # key2听牌，key1不听牌
            suggestion = "推荐" + int_2_pai[key1] + "和" + int_2_pai[key2] + \
                         "。但我觉得出" + int_2_pai[key2] + "更好。"
            contrast_explanation = (contrast_explanation +
                                    "下面两张牌都是根据概率计算推荐的出牌，但是出" +
                                    int_2_pai[key2] +
                                    "就听牌了，而出另外一张不能听牌。")
            out_card = key2

        elif (info[key1]["is_ting"] == info[key2]["is_ting"]) & info[key1]["is_ting"]:
            # 两张牌都听牌
            contrast_explanation = contrast_explanation + "出掉这两张牌后即可听牌。"
            if info[key1]["left_cards_num"] > info[key2]["left_cards_num"]:
                # key1听牌的数量大于key2
                suggestion = "推荐" + int_2_pai[key1] + "或" + int_2_pai[key2] + \
                             "。不过我觉得出" + int_2_pai[key1] + "好。"
                contrast_explanation = (contrast_explanation +
                                        "出掉" + int_2_pai[key1] +
                                        "后，可以摸" + array_2_card(info[key1]["left_cards"]) +
                                        "这些牌在牌池中剩余有" + str(info[key1]["left_cards_num"]) +
                                        "张，比出掉" + int_2_pai[key2] +
                                        "后，牌池中可以胡牌的牌的数量更多。")
                out_card = key1

            elif info[key1]["left_cards_num"] < info[key2]["left_cards_num"]:
                # key1听牌的数量大于key2
                suggestion = "推荐出" + int_2_pai[key1] + "或" + int_2_pai[key2] + \
                             "。但我觉得出" + int_2_pai[key2] + "更好。"
                contrast_explanation = (contrast_explanation +
                                        "出掉" + int_2_pai[key2] +
                                        "后，可以摸" + array_2_card(info[key2]["left_cards"]) +
                                        "这些牌在牌池中剩余有" + str(info[key2]["left_cards_num"]) +
                                        "张，比出掉" + int_2_pai[key1] +
                                        "后，牌池中可以胡牌的牌的数量更多。")
                out_card = key2

            else:
                # 两张牌听牌的数量相等
                if info[key1]["left_cards_num"] != 0:
                    # 且听牌数量均不为 0， 则比较概率分值。
                    contrast_explanation = contrast_explanation + "且听牌的数量相等。"
                else:
                    # 且听牌数量均为 0， 比较概率分值。
                    contrast_explanation = contrast_explanation + "因为"

                if info[key1]["p"] > info[key2]["p"]:
                    suggestion = "推荐的牌有" + int_2_pai[key1] + "或" + int_2_pai[key2] + \
                                 "。但" + int_2_pai[key1] + "更好。"
                    contrast_explanation = (contrast_explanation +
                                            "但出" + int_2_pai[key1] +
                                            "获胜的机会更大。")
                    out_card = key1

                elif info[key1]["p"] < info[key2]["p"]:
                    suggestion = "建议" + int_2_pai[key1] + "或" + int_2_pai[key2] + \
                                 "。不错出" + int_2_pai[key2] + "更好。"
                    contrast_explanation = (contrast_explanation +
                                            "但出" + int_2_pai[key2] +
                                            "获胜的概率更大。")
                    out_card = key2

                else:
                    suggestion = "可以考虑" + int_2_pai[key1] + "或" + int_2_pai[key2] + \
                                 "这两张牌各方面都差不多。"
                    contrast_explanation = (contrast_explanation +
                                            "两者概率相同，出哪一张都可以。")
                    out_card = key2

        else:
            # 两张牌都不听牌，这时要先考虑概率，再考虑桌面牌剩余。
            if info[key1]["p"] > (info[key2]["p"] + 0.5):
                # 如果key1的概率大于key2
                suggestion = "推荐的牌有" + int_2_pai[key1] + "或" + int_2_pai[key2] + \
                             "。但是更建议出" + int_2_pai[key1] + "。"
                contrast_explanation = (contrast_explanation +
                                        "出掉" + int_2_pai[key1]  +
                                        "后，牌的获胜概率显著大于出掉" +
                                        int_2_pai[key2] + "。")
                out_card = key1
            elif info[key2]["p"] > (info[key1]["p"] + 0.5):
                # 如果key2的概率大于key1
                suggestion = "可以考虑" + int_2_pai[key1] + "或" + int_2_pai[key2] + \
                             "。不过" + int_2_pai[key2] + "更好一点。"
                contrast_explanation = (contrast_explanation +
                                        "出掉" + int_2_pai[key2]  +
                                        "后，牌的获胜概率显著大于出掉" +
                                        int_2_pai[key1] + "。")
                out_card = key2
            else:
                # 两者概率接近，则考虑进张的牌
                contrast_explanation = contrast_explanation + "两张牌在概率上比较接近。"
                if info[key1]["left_cards"] and info[key2]["left_cards"]:
                    if info[key1]["left_cards_num"] > info[key2]["left_cards_num"]:
                        suggestion = "推荐" + int_2_pai[key1] + "或" + int_2_pai[key2] + \
                                     "。但" + int_2_pai[key1] + "更好。"
                        contrast_explanation = (contrast_explanation +
                                                "出" + int_2_pai[key1] + "后，会拥有更大的进张机会。可以摸" +
                                                array_2_card(info[key1]["left_cards"]) +
                                                "，这些牌剩余" + str(info[key1]["left_cards_num"]) +
                                                "张，比出另外一张牌更好。")
                        out_card = key1
                    elif info[key2]["left_cards_num"] > info[key1]["left_cards_num"]:
                        suggestion = "建议" + int_2_pai[key1] + "或" + int_2_pai[key2] + \
                                     "。但更推荐出" + int_2_pai[key2] + "。"
                        contrast_explanation = (contrast_explanation +
                                                "出" + int_2_pai[key2] + "后，会拥有更大的进张机会。可以摸" +
                                                array_2_card(info[key2]["left_cards"]) +
                                                "，这些牌剩余" + str(info[key2]["left_cards_num"]) +
                                                "张，比出另外一张牌更好。")
                        out_card = key2
                    else:
                        out_card = key1
                        if info[key2]["p"] > info[key1]["p"]:
                            out_card = key2
                        suggestion = "可以出" + int_2_pai[key1] + "和" + int_2_pai[key2] + \
                                     "。这两张差不多。"
                        contrast_explanation = (contrast_explanation +
                                                "两张牌差不多，出哪一张都可以。")
                elif info[key1]["left_cards"] and not info[key2]["left_cards"]:
                    out_card = key1
                    suggestion = "推荐" + int_2_pai[key1] + "或" + int_2_pai[key2] + \
                                 "。但" + int_2_pai[key1] + "更好。"
                    contrast_explanation = (contrast_explanation +
                                            "出" + int_2_pai[key1] + "后，会拥有更多的进张机会。可以摸" +
                                            array_2_card(info[key1]["left_cards"]) +
                                            "这些牌。而出另外一张则没有较好可以进张的牌")
                elif info[key2]["left_cards"] and not info[key1]["left_cards"]:
                    out_card = key2
                    suggestion = "可以出" + int_2_pai[key1] + "或" + int_2_pai[key2] +\
                                 "。但是出" + int_2_pai[key2] + "更好。"
                    contrast_explanation = (contrast_explanation +
                                            "出" + int_2_pai[key2] + "后，会拥有更多的进张机会。可以摸" +
                                            array_2_card(info[key2]["left_cards"]) +
                                            "这些牌。而出另外一张则没有较好可以进张的牌")
                else:
                    out_card = key1
                    suggestion = "推荐" + int_2_pai[key1] + "或" + int_2_pai[key2] +\
                                 "这两张牌出哪张都可以。"
                    contrast_explanation = (contrast_explanation +
                                            "两张牌各方面相似，出任意一张即可。")

    # 编辑deductive
    random_sug = ["我觉得出", "出", "建议出", "我推荐", "应该出", "我认为应该出"]
    de_suggestion = ""
    if out_card != -1:
        de_suggestion = random_sug[random.randint(0, 5)] + int_2_pai[out_card]
        if info[out_card]["is_ting"]:
            deductive_explanation = (deductive_explanation +
                                     "出掉这张牌后即可听牌，可以听的牌有" +
                                     array_2_card(info[out_card]["left_cards"]) +
                                     "，这些牌在牌池中剩余" +
                                     str(info[out_card]["left_cards_num"]) +
                                     "张，因此推荐出" +
                                     int_2_pai[out_card] + "。")
        else:
            deductive_explanation = (deductive_explanation +
                                     "出掉这张牌后，获胜的概率比较高。因为可以继续摸" +
                                     array_2_card(info[out_card]["left_cards"]) +
                                     "形成更好的牌形，这些牌在牌组中剩余一共有" +
                                     str(info[out_card]["left_cards_num"]) +
                                     "张，摸到的几率非常高，因此推荐出" +
                                     int_2_pai[out_card] + "。")

    return {"out_card": out_card,
            "suggestion": suggestion,
            "de_suggestion": de_suggestion,
            "deduction": deductive_explanation,
            "contrast": contrast_explanation,
            "card_info": info, }


def normal_out():
    card_info = alt_card()
    result = select_explain(card_info)
    result_explanation_path = "C:/Users/clark/MahjongGame/XAIMethod-AutoLevel/ResultLogs/Explanation.json"
    with open(result_explanation_path, "w") as save_obj:
        json.dump(result, save_obj)
    return result["out_card"]


def nao_speak():
    suggestion_path = "C:/Users/clark/MahjongGame/XAIMethod-AutoLevel/ResultLogs/Explanation.json"
    with open(suggestion_path, 'r') as load_f:
        json_str = load_f.read()
    if len(json_str) > 0:
        suggestion = json.loads(json_str)
        text = suggestion["de_suggestion"].encode("utf8")
        tts = ALProxy("ALTextToSpeech", "169.254.167.207", 9559)
        tts.say(text)
    return
