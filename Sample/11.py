import random


class Tile:
    def __init__(self, value, flag=None):
        self.value = value
        self.flag = flag

    def __repr__(self):
        if self.flag is None:
            return f"Tile({self.value})"
        else:
            return f"Tile({self.value}, {self.flag})"


rows, cols = 15, 30
array = []

for _ in range(rows):
    # 随机选两列
    indices = random.sample(range(cols), 2)
    row = []
    for i in range(cols):
        if i in indices:
            row.append(Tile(6, False))
        else:
            row.append(Tile(0))
    array.append(row)

# array 是你要的二维数组，以下代码可以打印查看
for row in array:
    print(row, end=",")
