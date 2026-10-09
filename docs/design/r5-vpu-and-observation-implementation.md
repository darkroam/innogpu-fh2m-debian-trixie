# R33/R34 VPU契约修正与R5观测实现

状态：R33/R34已由22f7548收档；R35/R36接续见§4。2026-09-24；原实现批实机实验0次。
当前运行结论仍以[status](../project/status.md)为准，本页不是安装或实验放行。

## 1. i11最小派生候选

身份：`5.0.0-i11`，固定epoch `1790208000`；i6→i9→i10既有整树门不变，
i10父树`c08b2bd426bcceba2853eb0767994875b3080cfaf4d5a1afcff821f3092aa67e`之后仅修改
`fantvpu/fantvpu_drv.c`三处：首次arm前赋既有`timer_suspend=false`；PREPARE和stop两处将
退出条件从`ret == 0`改为`ret == 0 || ret == 1`。派生整树实测：
`f0e5490ad2d14fb280ef0e3bf76018462143b9223a357e0699b1c0ed376980b5`。

[生产builder](../../tools/build-innogpu-driver.sh)编译与包内源码共用派生函数；版本窄门仅i11，
i1–i10、未知版本、错epoch、输入漂移继续拒绝。默认Deepin版本不变，不回写任何原物。
没有改shared dev_rsrc、宏配置、回调路由、锁、POST次序或对象发布/注销时序。

这是D/F共享的未初始化/返回契约问题，不能称为F独有timer死锁；API正常返回0/1，不是EINTR重试。
保留原循环的异常值防御形态，只在正常0/1返回时结束；同步删除内部等待并未改变。
并发rearm、callback生存期与闭源callback是否终止仍是旧风险，未以本次小改证明普遍并发正确。
VPU全局初始化在首次设备注册通知之前开始；remove中的注销仍晚于部分拆除，这是单独生命周期风险，
本批不扩大修改或宣称已解决。用户态mock不模拟Linux调度或真实设备。

开发验证：生产派生源抽取原函数，非零填充内存、分配失败、空/重复通知、suspend/hibernate、stop，
删除返回0/1均测试；输入清理等既有回归继续跑。101单核离线模块编译与strip前ABI检查为开发成绩，
**不是正式八核A/B、包签署或安装资格**。i10历史正式runner/证据保留原身份，不机械改名为i11。
双轨纪律的腿B尚未生成：本批`patches/`冻结只读，当前为派生源码开发候选；
进入发布流程前仍须另行解锁并补齐patch/meta与重放印证，不能以builder派生记录替代。

## 2. r5obs1实现与记录边界

[离线源码准备工具](../../tools/internal/prepare-r5-observation.py)只接受锁定的6.12.101归档输入，
新建独立副本；不复制`.pem/.key`等签名材料、不修改冻结源。变更仅四个文件：

| 文件 | 实现 | 不改变的行为 |
|---|---|---|
| kernel/notifier.c | 对实际间接notifier调用加成对entry/exit，记录callback、nb、action及ret | 原调用/STOP_MASK/robust rollback与返回值 |
| include/trace/events/notifier.h | notifier_boundary事件定义 | 原notifier_run事件保留 |
| drivers/base/power/main.c | dpm_prepare局部attempt/moved计数、did_move与原始ret；在-EAGAIN清零前保留ret | list移动条件、重试、锁和错误分支 |
| include/trace/events/power.h | r5_prepare_progress事件定义 | 原设备回调与suspend_resume事件 |

使用Linux现成function_graph和tracepoint环形缓冲，不新建自制队列/发送线程/watchdog。
固定release提案`6.12.101-r5obs1`；以现有101头的config为输入，LOCALVERSION独立，构建测试证书
由Kbuild使用自带显式配置生成，私钥不读取/导出且构建结束删除；测试证书不构成宿主信任。
不安装内核、不加载观测模块、不触发PM。当前普通101驱动模块不能直接冒充新release的兼容模块。

[离线配置/检查工具](../../tools/internal/r5-observation.py)从System.map选唯一实际text symbol，
生成JSON规格，不执行tracefs写入。部分小函数被内联时在外层dpm_prepare与依赖等待根覆盖，
不是凭空要求不存在的device_prepare/dpm_wait符号；实际ftrace可用性仍须未来预检，缺失即停。

| 观测面 | 接线 |
|---|---|
| console / notifier | pm_prepare_console、suspend_console函数图；框架notifier_boundary完整entry/exit，含其他设备和robust rollback |
| probe/runtime/等待者 | probe/async/barrier外层函数图；rpm事件、sched_switch/wakeup；动态参数探针记录device/work/completion对象和probe_count |
| prepare重试进度 | 设备回调start/end + 新progress事件；区分ret0但无list entry、-EAGAIN且moved不增、失败退出 |
| VPU与HAL | 实际加载驱动符号必须核验，加入函数图；timer_expire_entry/exit覆盖包括VPU在内的回调；间接目标仍UNKNOWN直到现场解出 |
| 丢失/截断 | perCPU overrun/commit_overrun/dropped、function graph overrun、probe nmissed全收；缺任一不判覆盖成立 |

缓冲按每CPU1024KiB预分配，总预算65536KiB，超过CPU预算或分配失败即停，不静默缩小覆盖。
关闭overwrite以保留早期事件；满后仍可drop，因此必须读计数。IRQ路径不新增睡眠/动态分配/printk/网络发送。
tracepoint事件的时间/CPU/PID由现成记录器提供；notifier nb/callback/action按task调用栈配对，
普通task允许迁核，PID 0的idle按CPU隔离；错序exit拒绝，未返回entry保留。
多CPU时间关联不是严格因果顺序，probe_count采样也不与全局事件排序原子；分析须保留这个限制。

检查器仅接受选定的规范化notifier/prepare记录加完整丢失计数，保留未配对entry，报告连续-EAGAIN无进度候选。
它不解析任意raw图或自动判根因；原始trace、图、设备身份、通道正样需一同审查，不能用规范化摘要覆盖原件。

## 3. 放行、恢复与未完成条件

观测规格是供后续实施/审查使用的普通文件，不是自动加载器。未来需逐项落实精确内核/驱动身份、
实际ftrace符号与事件、动态探针目标、接收机、预算/丢失、正样、起止标记和回退卡，才能谈实验条件。
UART物理接线/电平/接收端、NIC/netpoll/PM存活及独立落盘目前仍UNVERIFIED；
**本地RAM环形缓冲无法保证硬挂或冷断电保全，无通道不靠重复watchdog碰运气。**

本批恢复仅为丢弃新源码/编译副本及撤回代码修订，原运行包/GRUB/DKMS均未动。
安装、重启、首次运行观测器与任何PM实验须按决策边界另批授权，当前禁止重跑保持。
OUTSIDE_COVERAGE、R5=FAIL、禁止重跑、U1/U2、validation-results、未打 tag；1C不变，root cause=unresolved。

## 4. R35/R36接续与前置补正（2026-09-24）

用户在R33/R34收档后明确授权i11正式离线A/B及r5obs1安装+一次覆盖观测，包含通道和保全前置。
上文未授权描述对应原批历史；本次一次观测授权不等于重复watchdog/pm_test全面解冻，
也不证明独立接收、恢复或精确运行身份已经成立。无通道正样或恢复条件，现场窗口仍NOT_RUN。

R35使用R30封存i10前驱完成八核A/B，整deb SHA-256为
`254a165e6f0ec8deb5bf6dab23346ecc9c7a45525adb99c62bfa6988e4eb6c92`；
逐核ABI/签署/模块/initrd与真实升级链已离线通过，成果待qoder/dsh审查，不代替宿主运行证据。

R36前置复核发现R34镜像的`CONFIG_FUNCTION_GRAPH_RETVAL`未启用，而规格包含`funcgraph-retval`。
16个根含fentry不能证明图返回值功能存在。离线工具改为
`plan System.map --config .config`，要求精确release及返回值/图/事件/探针配置均成立；
旧镜像被该门拒绝，原哈希与开发成绩保留。修正配置须重新编译独立副本、生成匹配模块并单列身份。

修订副本现已完成离线构建：bzImage SHA-256
`b8d1eb98eb7d1dd49802e03a3765eea7b33f7e100198fdef38da464938912fab`，
匹配i11模块`99da915e9fa91b09e263180db96f9404ab0e6e0a399cce7710c9b1f576333673`。
config仅启用FUNCTION_GRAPH_RETVAL与MODULE_SIG_ALL；内核16/驱动7个根的入口fentry已离线核验，
暂存模块4228个；ABI为size=140536/members=115/offset=140528，临时签名私钥已删除。
构建中断的原副本与后验工具PATH失败均保留，新副本/新后验目录单列，未拼成一次正式A/B。
这些是观测准备产物，尚无实际加载/宿主信任/完整外传正样，不是安装验收或实验通过。

保全还存在传输边界：用户态trace_pipe/SSH消费者会随进程冻结停止，普通网络可达不证明覆盖freeze之后。
tracepoint的`tp_printk`会引入全局锁和printk路径，且不等价导出完整function_graph；
不得直接打开它来冒充既定一次覆盖方案。USB网卡发送依赖USB主控与异步完成，
仅连通网关也不能确认硬挂时的可用性。独立接收程序/链路、非PM正样、传输与被观测等待的依赖关系
仍须实测并审清；本地RAM丢失风险未解除，安装和PM尚未执行。

### 4.1 R36完整传输补充

原生ftrace export接口不包含function_graph，不能把普通UDP正样当作完整外传。
现由生成器产出一个独立GPL-2.0-only模块：锁定trace.h内容、精确release和私有instance，
消费原生环形缓冲，复用非freezable的system_unbound_wq和netpoll；不新增发送线程/watchdog，
不在被观测回调中发包。模块拒绝正在记录/已有消费者/overwrite/非function_graph实例，
总环形内存上限64MiB，最多64 CPU；每次最多256条、每条1280字节，发送队列阈值64。
循环每次消费前检查5ms期限；底层netpoll单次发送及统计阶段不具有硬实时5ms保证。

报文含session/sequence/CPU/worker PID/时间/丢失与超长计数；BEGIN声明界限，
每秒逐CPU统计，停止记录且排空后卸载发送末尾统计与END。所有配置与读取者须由单一执行器独占，
禁止另开trace_pipe、重置/快照/改过滤；模块的检查不是与任意root并发修改之间的完整互斥。
Windows原始报文CreateNew落盘，约100ms持久flush；严格接收方序号/身份/损失/END检查另行执行。
两套窗口使用不同nonce和文件，控制正样不能拼作正式窗口。

此路径不依赖SSH用户进程，但仍依赖可调度CPU、内核timer、netpoll及网卡未停机；
全局硬锁、非冻结worker无法运行或NIC进入设备挂起均可能使尾部丢失。
没有END、缺序号、溢出或缺末尾统计一律不能报完整；只有已接收前缀仍可作为有界证据。
wire-check只判断传输；事件format、boot与build ID、graph overrun和probe nmissed必须另外核。
非PM完整链正样须在新内核实际加载后完成，不能用离线55项检查替代。
Linux 6.12的graph根过滤为全局配置，私有instance不隔离该过滤器；须独占并保留前值。
运行时按available_filter_functions_addrs逐地址解析，驱动同名物理入口全部保留，不能折叠后声称唯一。

导出模块加入新的统一签署集合；旧build-02及临时证书身份保留，私钥已删除不恢复。
新副本内核与全部4229模块签署同一新证书，公钥及模块签名可核，生成私钥已删除。
这是R36准备构建，不是重跑R35正式A/B、修改旧成绩或发布资格。
