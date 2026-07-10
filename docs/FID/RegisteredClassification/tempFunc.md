输入图像
   ↓
ROI 定位 / 位置修正
   ↓
目标区域裁剪
   ↓
姿态归一化：平移、旋转、尺度统一
   ↓
综合特征提取
   ↓
综合特征归一化
   ↓
MLP 分类
   ↓
输出类别 + 置信度 + OK/NG 判断

2. 一级：定位和归一化
find_shape_model(Image, ModelID, AngleStart, AngleExtent, MinScore, NumMatches, ...
                 Row, Column, Angle, Score)

vector_angle_to_rigid(Row, Column, Angle, RefRow, RefCol, 0, HomMat2D)

affine_trans_image(Image, ImageAffine, HomMat2D, 'constant', 'false')