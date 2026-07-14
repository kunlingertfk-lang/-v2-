那就按少样本注册分类来做，目标是每类只用少量样本，也能接近海康“注册分类”的使用体验。

推荐第一版架构：

图像采集
→ 位置定位
→ ROI 对齐
→ 特征提取
→ 类别模板库
→ 相似度计算
→ 阈值拒识
→ 输出类别
一、不要先上 MLP

情况 A 通常每类只有 1～10 张样本，MLP 很容易过拟合。更合适的是：

深度特征 embedding
+
最近邻 / 类别中心
+
SVM 作为可选增强

优先级建议：

预训练网络特征 + 最近邻
预训练网络特征 + 类别中心
预训练网络特征 + SVM
手工特征 + SVM

其中，第 1 种最像“注册几张图立即使用”。

二、推荐实现方案
1. 先定位并对齐

同一个产品在图像中必须尽量保持：

中心一致
角度一致
尺寸一致
ROI 大小一致
背景范围一致

HALCON 可使用形状模板定位：

create_shape_model (...)
find_shape_model (...)
vector_angle_to_rigid (...)
affine_trans_image (...)

之后裁剪固定 ROI：

gen_rectangle1 (ROI, Row1, Col1, Row2, Col2)
reduce_domain (AlignedImage, ROI, ImageReduced)
crop_domain (ImageReduced, ImageCrop)
zoom_image_size (ImageCrop, ImageResized, 224, 224, 'constant')

如果目标本身位置已经由机械治具固定，也可以暂时不做模板定位，但 ROI 必须固定。

2. 每张注册图提取一个特征向量

推荐使用轻量预训练网络的中间层输出作为 embedding，例如：

224 × 224 图像
→ MobileNet / ResNet18
→ 128～1024 维特征向量

不要直接使用最终 Softmax 分类层，因为你要支持动态增加类别。

理想的特征应满足：

同类样本距离小
异类样本距离大

常用距离：

余弦相似度
s(x,y)=
∥x∥∥y∥
x⋅y
	​


数值越接近 1，越相似。

欧氏距离
d(x,y)=
i
∑
	​

(x
i
	​

−y
i
	​

)
2
	​


数值越小，越相似。

对于深度 embedding，建议先做 L2 归一化，再用余弦相似度。

三、模板库怎么组织

假设有 3 个类别：

A 类：A1, A2, A3
B 类：B1, B2, B3
C 类：C1, C2, C3

每张注册图提取一个特征：

A 类：fA1, fA2, fA3
B 类：fB1, fB2, fB3
C 类：fC1, fC2, fC3

有两种判定方式。

方法 1：最近邻

待测特征为 q，计算它和所有注册样本的相似度：

s(q,f
ij
	​

)

取最高相似度对应的类别：

c
^
=arg
i,j
max
	​

s(q,f
ij
	​

)

优点：

注册新样本不需要重新训练
最符合“注册分类”
可保留每个类别的不同外观状态

缺点：

模板数量增加后，计算量上升
异常注册图会影响结果

每类只有 1～20 张时，这个计算量通常完全可以接受。

方法 2：类别中心

每个类别的特征取平均：

μ
c
	​

=
N
c
	​

1
	​

i=1
∑
N
c
	​

	​

f
c,i
	​


待测特征与各类别中心比较：

c
^
=arg
c
max
	​

s(q,μ
c
	​

)

优点：

速度快
模型小
不容易被单个异常样本影响

缺点：

当同一类别存在多个差异明显的姿态时，平均中心可能不理想

推荐策略是：

类内差异小：类别中心
类内差异大：最近邻

也可以每类做 2～3 个聚类中心。

四、必须做拒识

注册分类不能只输出“最像哪个类”，还必须判断“是否真的属于已知类别”。

建议用三层条件。

条件 1：最高相似度
最高相似度 >= MinScore

例如：

MinScore = 0.80

低于阈值输出：

UNKNOWN
条件 2：第一名与第二名差值
margin=s
1
	​

−s
2
	​


要求：

margin >= MinMargin

例如：

MinMargin = 0.08

若 A 类得分 0.88，B 类得分 0.87，虽然最高分不低，但类别不明确，也应拒识。

条件 3：类内距离约束

注册阶段计算该类别样本到类别中心的距离分布：

d
c,i
	​

=∥f
c,i
	​

−μ
c
	​

∥

设类别允许半径：

R
c
	​

=mean(d
c
	​

)+k⋅std(d
c
	​

)

通常：

k = 2～3

预测时：

待测样本到类别中心距离 > Rc
→ UNKNOWN

这样比所有类别共用一个固定阈值更稳定。

五、完整判定逻辑
提取查询特征 q

计算所有类别得分
取得最高得分 s1、第二高得分 s2
取得最佳类别 bestClass

若 s1 < MinScore
    UNKNOWN
否则若 s1 - s2 < MinMargin
    UNKNOWN
否则若到 bestClass 类中心距离 > 类别半径
    UNKNOWN
否则
    输出 bestClass

伪代码：

def classify(feature, class_templates, class_centers, class_radii):
    scores = {}

    for class_name, templates in class_templates.items():
        similarities = [
            cosine_similarity(feature, template)
            for template in templates
        ]
        scores[class_name] = max(similarities)

    ranked = sorted(scores.items(), key=lambda x: x[1], reverse=True)

    best_class, best_score = ranked[0]
    second_score = ranked[1][1] if len(ranked) > 1 else -1.0

    if best_score < 0.80:
        return "UNKNOWN", best_score

    if best_score - second_score < 0.08:
        return "UNKNOWN", best_score

    distance = euclidean_distance(feature, class_centers[best_class])

    if distance > class_radii[best_class]:
        return "UNKNOWN", best_score

    return best_class, best_score
六、注册样本怎么拍

每类建议至少注册 3～5 张，不建议只有 1 张。

每类可以覆盖：

标准位置
轻微左偏
轻微右偏
亮度稍高
亮度稍低
正常批次差异

但不要把严重异常图注册进正常类别。

建议：

类别难度	每类注册数
差异非常明显	3～5
普通产品分类	5～10
类间差异较细	10～20

如果每类只有 1 张，系统只能做模板匹配式识别，对光照和批次变化会非常敏感。

七、颜色场景建议增加手工特征

如果分类主要依赖颜色，仅使用深度特征不一定最稳。可以拼接颜色特征：

F=[F
deep
	​

,F
Lab
	​

,F
HSV
	​

]

例如：

深度特征：256 维
Lab 均值和标准差：6 维
HSV 直方图：48 维
总特征：310 维

拼接后分别标准化，再做 L2 归一化。

例如：

final_feature =
0.7 × deep_feature
+
0.3 × color_feature

更规范的做法是分别计算两个相似度：

S=w
d
	​

S
deep
	​

+w
c
	​

S
color
	​


初始可以设置：

深度相似度权重 0.7
颜色相似度权重 0.3

如果产品主要靠颜色区分，可调成：

0.4 / 0.6
八、第一版产品功能建议

做成一个和海康接近的界面时，可以提供：

注册阶段
新增类别
类别命名
采集注册图
删除异常注册图
显示类内相似度
生成类别模板
运行阶段
预测类别
最高相似度
第二类别
类别差值
OK / UNKNOWN
推理耗时
可调参数
最低相似度
最低类别差值
特征类型
颜色权重
是否启用亮度特征
位置修正

这也能和你前面研究的海康颜色识别、颜色比较逻辑统一起来。

九、建议先做的最小可用版本
固定 ROI
→ 224×224 缩放
→ 预训练网络提取 embedding
→ L2 归一化
→ 每类注册 5 张
→ 最近邻余弦相似度
→ 最高分阈值
→ 第一第二名分差
→ UNKNOWN

第一版先不要加入 MLP。验证效果后，再逐步增加：

位置修正
颜色特征融合
类别中心
类内半径
难例追加
SVM

这条路线实现成本低、可解释性强，也最接近“少量图片注册后即可分类”的工业产品体验。

