import time
import numpy as np
import queue
import prettytable as pt
# 表格化结果

# 创建Node类 (包含当前数据内容,父节点,步数)
class Node:
    inspire = -1  # 启发值
    step = 0  # 初始状态到当前状态的距离（步数）
    parent = None,  # 父节点

    # 用状态和步数构造节点对象
    def __init__(self, data, step, parent):
        self.data = data        # 当前状态数值,是一个数组
        self.step = step        # 当前的步数
        self.parent = parent    # 当前节点的父节点
        self.inspire = cost(data) + step  # 计算启发值

# 定义一个全局变量来表示目标状态
end_data = [[],[],[]] 

def initial():
    '''初始化，用户输入初始状态和目标状态'''
    flag = True
    while flag:
        flag = False
        List1 = []
        judge = [0, 0, 0, 0, 0, 0, 0, 0, 0]
        temp = input("请输入初始状态（以空格隔开，例如：0 1 2 3 4 5 6 7 8）:")
        List1 = temp.split(" ")
        for i, value in enumerate(List1):
            List1[i] = int(value)
            if (List1[i] >= 0 and List1[i] <= 8):
                judge[List1[i]] += 1

        while i in range(9):
            if judge[i] != 1:
                print("输入错误!")
                flag = True
                break
            i += 1
        
    start_data = np.array([[List1[0], List1[1], List1[2]], [
                          List1[3], List1[4], List1[5]], [List1[6], List1[7], List1[8]]])
    flag = True
    global end_data
    while flag:
        flag = False
        List1 = []
        judge = [0, 0, 0, 0, 0, 0, 0, 0, 0]
        temp = input("请输入目标状态（以空格隔开，例如：0 1 2 3 4 5 6 7 8）:")
        List1 = temp.split(" ")
        for i, value in enumerate(List1):
            List1[i] = int(value)
            if (List1[i] >= 0 and List1[i] <= 8):
                judge[List1[i]] += 1
        while i in range(9):
            if judge[i] != 1:
                flag = True
                print("输入错误!")
                break
            i += 1
        
    end_data[0]=np.array([List1[0],List1[1],List1[2]])
    end_data[1]=np.array([List1[3],List1[4],List1[5]])
    end_data[2]=np.array([List1[6],List1[7],List1[8]])
    return start_data

def find_zero(num):
    '''寻找空格(0)号元素的位置'''
    tmpx, tmpy = np.where(num == 0)
    # 返回0所在的x坐标与y坐标
    return tmpx[0], tmpy[0]

def swap(num_data, direction):
    '''一次交换'''
    x, y = find_zero(num_data)
    num = np.copy(num_data)
    if direction == 'left':
        if y == 0:
            return num
        num[x][y] = num[x][y - 1]
        num[x][y - 1] = 0
        return num
    if direction == 'right':
        if y == 2:
            return num
        num[x][y] = num[x][y + 1]
        num[x][y + 1] = 0
        return num
    if direction == 'up':
        if x == 0:
            return num
        num[x][y] = num[x - 1][y]
        num[x - 1][y] = 0
        return num
    if direction == 'down':
        if x == 2:
            return num
        else:
            num[x][y] = num[x + 1][y]
            num[x + 1][y] = 0
            return num

def cost(num):
    '''计算此时的不同程度，即h(n)'''
    con = 0
    # 遍历整个九宫格
    # i = 0
    # j = 0
    for i in range(3):
        for j in range(3):
            tmp_num = num[i][j]
            compare_num = end_data[i][j]
            if tmp_num != 0:
                # 不等则加一
                con += (tmp_num != compare_num)
    return con

def data_to_int(num):
    '''将data转化为不一样的数字便于比较'''
    value = 0
    for i in num:
        for j in i:
            value = value * 10 + j
    # 1 2 3 4 5 6 7 8 0会得到123456780
    return value

def sort_by_inspire(opened):
    '''给open表按照启发值排序'''
    tmp_open = opened.queue.copy()
    length = len(tmp_open)
    # 排序,从小到大,当一样的时候按照step的大小排序
    for i in range(length):
        for j in range(length):
            if tmp_open[i].inspire < tmp_open[j].inspire:
                tmp = tmp_open[j]
                tmp_open[j] = tmp_open[i]
                tmp_open[i] = tmp
            if tmp_open[i].inspire == tmp_open[j].inspire:
                if tmp_open[i].step > tmp_open[j].step:
                    tmp = tmp_open[j]
                    tmp_open[j] = tmp_open[i]
                    tmp_open[i] = tmp
    opened.queue = tmp_open

def refresh_open(now_node, opened):
    '''编写一个比较当前节点是否在open表中,如果在,根据f(n)的大小来判断去留'''
    # 复制一份open表的内容
    tmp_open = opened.queue.copy()
    for i in range(len(tmp_open)):
        # 这里要比较一下node和now_node的区别,并决定是否更新
        data = tmp_open[i]
        now_data = now_node.data
        # now_node在队列中
        if (data == now_data).all():
            data_inspire = tmp_open[i].inspire
            now_data_inspire = now_node.inspire
            # 原始的启发值要小，则不改变
            if data_inspire <= now_data_inspire:
                return False
            # 原始的启发值较大，那么需要更新
            else:
                print('')
                tmp_open[i] = now_node
                # 更新之后的open表还原
                opened.queue = tmp_open
                return True
    # 如果没找到，那么就将该节点加入队列
    tmp_open.append(now_node)
    # 更新之后的open表还原
    opened.queue = tmp_open
    return True

def output_result(node):
    '''根据最终节点获取整个路径'''
    all_node = [node]
    # 回溯父节点，加入到列表中
    for i in range(node.step):
        father_node = node.parent
        all_node.append(father_node)
        node = father_node
    # 返回反转后的列表
    return reversed(all_node)

def print_result(result_node):
    '''将整个过程输出'''
    node_list = list(output_result(result_node))
    tb = pt.PrettyTable()
    tb.field_names = ['step', 'data', 'inspire']
    for node in node_list:
        num = node.data
        tb.add_row([node.step, num, node.inspire])
        if node != node_list[-1]:
            tb.add_row(['---', '--------', '---'])
    print(tb)

def astar_method_a_function(opened, closed):
    '''AStar启发式搜索'''
    count = 0
    # 队列不为空
    while len(opened.queue) != 0:
        # 出队
        node = opened.get()
        # 如果出队节点是目标状态
        if (node.data == end_data).all():
            print(f'总共耗费{count}轮')
            return node
        # 将取出的点加入closed表中，标记已经访问
        # closed[data_to_int(node.data)] = 1
        closed.append(data_to_int(node.data))
        # 四种移动方法
        for action in ['left', 'right', 'up', 'down']:
            # 创建子节点
            child_node = Node(swap(node.data, action), node.step + 1, node)
            index = data_to_int(child_node.data)
            # 没有被访问
            if index not in closed:
                # 入队，但是要先判断是否在队列里
                refresh_open(child_node, opened)
        # 根据其中的inspire值为open表进行排序
        # 保证先出队的都是启发值较小的
        sort_by_inspire(opened)
        count += 1
        if count > 5000:
            print("没有搜索到可行解!")
            break

def astar_main(start_data):
    '''A*算法main函数'''
    # open表
    opened = queue.Queue()
    # 初始节点
    start_node = Node(start_data, 0, None)
    # 初始节点入队
    opened.put(start_node)
    # close表
    closed = []
    # 启发式搜索
    result_node = astar_method_a_function(opened, closed)
    # 输出结果
    print_result(result_node)

con = 0  # 全局变量

def dfs(node, closed, depth, count):
    '''dfs搜索，最大深度为count'''
    # 达到最大深度了，就返回
    if depth == count:
        return []
    # 计数
    global con
    con += 1
    # 四个方向分别查找
    for action in ['left', 'right', 'up', 'down']:
        child_node = Node(swap(node.data, action), node.step + 1, node)
        # 如果查找到了，那么就返回
        if (child_node.data == end_data).all():
            print(f'总共耗费{con}轮')
            return child_node
        # 得到此时九宫棋盘对应的唯一数字
        index = data_to_int(child_node.data)
        # 如果没有标记过
        if index not in closed:
            # 标记
            closed.append(index)
            # dfs搜索下一个节点
            a = dfs(child_node, closed, depth + 1, count)
            # 如果返回值不是 [] ，说明找到了
            if a != []:
                return a
    return []

def ids_method_a_function(start_node):
    '''迭代深入搜索'''
    # count表示查找深度，这里我让查找深度从2增加到100，每次增加2
    for count in range(2, 101, 2):
        # 标记列表
        closed = []
        # 标记起始节点
        closed.append(data_to_int(start_node.data))
        # dfs搜索
        a = dfs(start_node, closed, 0, count)
        # 搜索到了
        if a != []:
            return a
    print("没有搜索到可行解!")

def ids_main(start_data):
    '''ids算法main函数'''
    start_node = Node(start_data, 0, None)
    # 启发式搜索
    result_node = ids_method_a_function(start_node)
    # 输出结果
    print_result(result_node)

def main():
    '''main函数'''
    # 初始化
    start_data= initial()
    start = time.time()
    ids_main(start_data)
    end = time.time()
    print(f"迭代深入搜索算法耗时 {end - start} s")
    print("\n")
    start = time.time()
    astar_main(start_data)
    end = time.time()
    print(f"A*算法耗时 {end - start} s")

# 函数入口
if __name__ == "__main__":
    main()
