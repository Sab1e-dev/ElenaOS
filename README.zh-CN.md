<div style="display: flex; align-items: center;">
  <img src="./assets/logo-light.png#gh-light-mode-only" />
  <img src="./assets/logo-dark.png#gh-dark-mode-only" />
</div>

[![STATUS](https://img.shields.io/badge/状态-开发中-red)](#)
[![License](https://img.shields.io/badge/开源协议-Apache%202.0-brightgreen.svg)](LICENSE)
[![GUI](https://img.shields.io/badge/GUI-LVGL-blue)](https://lvgl.io)
[![Script Engine](https://img.shields.io/badge/脚本引擎-JerryScript-orange)](https://jerryscript.net)
![release](https://img.shields.io/github/v/tag/ElenixOS/ElenixOS)
[![Simulator](https://img.shields.io/badge/模拟器-在线体验-success)](https://simulator.elenixos.com/wasm/latest/main.html)

<div>
  <a href="./README.md">English</a> | <a href="./README.zh-CN.md">简体中文</a>
</div>

---

## 简述

ElenixOS 是一款开源的智能手表操作系统，基于 LVGL 构建图形界面，表盘与应用程序运行在 JerryScript 驱动的统一脚本引擎（Script Engine）之上。

系统采用脚本化的应用运行模型，通过 JavaScript 描述 UI 结构与交互逻辑，并由底层原生代码负责图形渲染、事件调度与硬件资源管理，从而在资源受限的嵌入式平台上实现灵活、高效的应用开发体验。

ElenixOS 在设计上强调可移植性与可扩展性，提供清晰的抽象层与统一接口，便于移植至不同 MCU 与硬件平台。同时，表盘与应用程序共享同一运行环境和 API 体系，降低系统复杂度并提升整体一致性。

在用户体验方面，ElenixOS 的整体 UI 与交互风格参考 Apple Watch，注重动画流畅性、手势操作与界面层级关系，结合 LVGL 的动画系统与事件机制，力求在嵌入式设备上提供接近原生智能手表的视觉与交互体验。

ElenixOS 既适用于个人项目与学习研究，也适合作为可定制的智能穿戴系统基础框架，为开发者提供从底层到应用层的完整解决方案。

## 在线模拟器

ElenixOS 提供了一个在线模拟器，允许您直接在 Web 浏览器中体验系统，无需任何设置。您可以在这里访问最新版本的模拟器：
[在线模拟器](https://simulator.elenixos.com/wasm/latest/main.html)

## 快速开始

请参考[快速开始](https://elenixos.com/docs/getting_started/quick_start)。

## 参考文档

请参考 [ElenixOS 文档](https://elenixos.com)。

## 社区

- [GitHub Discussions](https://github.com/ElenixOS/ElenixOS/discussions)
- [腾讯频道 (中国社区)](https://pd.qq.com/s/2arlf3js7)

## 开源协议

本软件遵循 Apache License 2.0 开源协议发布。该协议允许个人和组织自由使用、修改及分发本软件及其衍生作品，可用于商业用途，但需保留原始版权声明、许可证文本及相关声明。软件按“原样”提供，不附带任何形式的担保。

## 第三方软件

ElenixOS 在开发过程中使用了以下开源项目，在此对相关作者和社区表示感谢：

- LVGL: https://lvgl.io
- JerryScript: https://jerryscript.net
- RemixIcon: https://remixicon.com
- cJSON: https://github.com/DaveGamble/cJSON
