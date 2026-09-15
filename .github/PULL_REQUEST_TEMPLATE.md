# Pull Request

## 变更说明

<!-- 一句话说清这个 PR 做了什么、为什么。不要只说"新增功能"。 -->

- 关联 Issue：Closes #

## 变更类型

- [ ] feat 新功能
- [ ] fix 缺陷修复
- [ ] refactor 重构（不改变外部行为）
- [ ] docs 文档
- [ ] test 测试
- [ ] chore 构建/工具
- [ ] perf 性能优化

## 怎么验证

<!-- 让评审人能照着复现。命令 + 预期结果。 -->

```bat
scripts\build.bat
scripts\run.bat test
scripts\run.bat sim
scripts\run.bat monitor
```

**预期**：

1.
2.

## 自审 checklist

> 逐条过 `docs/代码评审checklist.md`，把不适用的划掉。

- [ ] 构建通过、零警告
- [ ] 涉及协议/统计逻辑已补单元测试且全绿
- [ ] 协议或表结构变更已同步更新 `docs/`
- [ ] 没有在工作线程里碰 UI
- [ ] 跨线程自定义类型已 `Q_DECLARE_METATYPE` + `qRegisterMetaType`
- [ ] 持锁时不调用同样加锁的函数
- [ ] `new` 出来的对象都有回收路径
- [ ] 失败路径没有静默吞掉，且写了日志
- [ ] 命名与注释符合规范（注释说明"为什么"）
- [ ] `DEVLOG.md` / `CHANGELOG.md` / `BUGS.md` 已按需更新

## 风险与影响面

<!-- 这个改动可能影响什么？回滚方式是什么？ -->

- 影响模块：
- 回滚方式：`git revert <commit>`
- 已知遗留问题：

## 截图 / 日志

<!-- 界面改动或协议调试，贴截图或抓包/日志片段 -->
