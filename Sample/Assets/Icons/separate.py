import os
from PIL import Image

def split_image(image_path, save_path):
    # 打开图片
    img = Image.open(image_path)
    
    # 获取图片的宽度和高度
    width, height = img.size
    
    # 计算每个格子的宽度和高度
    grid_width = width // 4
    grid_height = height // 4
    
    # 循环将图像分割成 16 格，并保存每个格子
    for row in range(4):
        for col in range(4):
            # 计算当前格子的坐标
            left = col * grid_width
            upper = row * grid_height
            right = left + grid_width
            lower = upper + grid_height
            
            # 创建新的图像对象并粘贴当前格子的图像
            new_img = Image.new("RGBA", (grid_width, grid_height))
            new_img.paste(img.crop((left, upper, right, lower)), (0, 0))
            
            # 构建保存文件名，加上-行数-列数
            filename = os.path.basename(image_path)
            save_name = f"{os.path.splitext(filename)[0]}-{row + 1}-{col + 1}.png"
            
            # 保存图像并保留透明度
            new_img.save(os.path.join(save_path, save_name), format='PNG', save_alpha=True)

# 获取当前文件夹路径
current_path = os.getcwd()

# 遍历当前文件夹中的所有png文件
for file in os.listdir(current_path):
    if file.endswith(".png"):
        file_path = os.path.join(current_path, file)
        
        # 创建保存分割图像的文件夹
        save_folder = os.path.join(current_path, "split_images")
        os.makedirs(save_folder, exist_ok=True)
        
        # 分割图像并保存所有格子
        split_image(file_path, save_folder)
