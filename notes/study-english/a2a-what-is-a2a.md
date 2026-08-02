# 🤝 What is A2A? ／ 什么是 A2A 协议？

📚 Bilingual (English-led) · Protocol Deep-Dive ｜ ~12 min ｜ Source: a2a-protocol.org

You know the feeling when your AI assistant has to book an international flight, arrange a hotel, and plan the itinerary all at once? Behind the scenes, several "specialist agents" divvy up the work. They don't natively speak the same language — so how do they connect? That is exactly the problem the A2A protocol solves.

🌐 中文：你有没有想过，当你的 AI 助手要帮你在网上订一张国际机票、同时安排酒店和行程时，背后其实是好几个"专家智能体"在分工协作？它们彼此语言不通，怎么打通？这正是 A2A 协议要解决的问题。

---

## 🧩 What is A2A? ／ 什么是 A2A？

The A2A protocol is an open standard that enables seamless communication and collaboration between AI agents. It provides a common language for agents built using diverse frameworks and by different vendors, fostering interoperability and breaking down silos. Agents are autonomous problem-solvers that act independently within their environment. A2A allows agents from different developers, built on different frameworks, and owned by different organizations to unite and work together.

🌐 中文：A2A 协议是一个开放标准，让 AI 智能体（agent）之间能够无缝通信与协作。它为那些用不同框架构建、由不同厂商开发的智能体提供了一套"通用语言"，从而提升互操作性、打破各自为战的"信息孤岛"。智能体是自主的问题解决者，在自身环境中独立行动。A2A 让来自不同开发者、基于不同框架、分属不同组织的智能体能够联合起来协同工作。

---

## 🎯 Why Use the A2A Protocol? ／ 为什么要使用 A2A 协议？

A2A addresses key challenges in AI agent collaboration. It provides a standardized approach for agents to interact. This section explains the problems A2A solves and the benefits it offers.

🌐 中文：A2A 解决了 AI 智能体协作中的关键难题，为智能体之间的交互提供了一套标准化方案。本节将说明 A2A 解决了哪些问题，以及它带来的好处。

### ⚠️ Problems that A2A Solves ／ A2A 解决什么问题

Consider a user request for an AI assistant to plan an international trip. This task involves orchestrating multiple specialized agents, such as:

🌐 中文：设想用户让 AI 助手规划一次国际旅行。这个任务需要协调多个专精型智能体，比如：

- A flight booking agent  
  🌐 中文：机票预订智能体
- A hotel reservation agent  
  🌐 中文：酒店预订智能体
- An agent for local tour recommendations  
  🌐 中文：本地游玩推荐智能体
- A currency conversion agent  
  🌐 中文：货币兑换智能体

Without A2A, integrating these diverse agents presents several challenges:

- **Agent Exposure**: Developers often wrap agents as tools to expose them to other agents, similar to how tools are exposed through the Model Context Protocol (MCP). However, this approach is inefficient because agents are designed to negotiate directly. Wrapping agents as tools limits their capabilities. A2A allows agents to be exposed as they are, without requiring this wrapping.  
  🌐 中文：开发者常把智能体包装成"工具"暴露给其他智能体，类似通过 MCP 暴露工具的做法。但这很低效，因为智能体本就设计为直接协商；包装成工具会限制其能力。A2A 允许智能体以其本来面貌暴露，无需这种包装。
- **Custom Integrations**: Each interaction requires custom, point-to-point solutions, creating significant engineering overhead.  
  🌐 中文：每次交互都需要点对点的定制方案，带来巨大的工程开销。
- **Slow Innovation**: Bespoke development for each new integration slows innovation.  
  🌐 中文：为每个新集成单独开发，拖慢创新节奏。
- **Scalability Issues**: Systems become difficult to scale and maintain as the number of agents and interactions grows.  
  🌐 中文：随着智能体和交互数量增长，系统变得难以扩展和维护。
- **Interoperability**: This approach limits interoperability, preventing the organic formation of complex AI ecosystems.  
  🌐 中文：这种方式限制了互操作，阻碍了复杂 AI 生态的自然形成。
- **Security Gaps**: Ad hoc communication often lacks consistent security measures.  
  🌐 中文：临时拼凑的通信往往缺乏一致的安全措施。

The A2A protocol addresses these challenges by establishing interoperability for AI agents to interact reliably and securely.

🌐 中文：A2A 协议通过为 AI 智能体建立可靠且安全的互操作能力，正面解决了这些挑战。

### 🧪 A2A Example Scenario ／ A2A 示例场景

This section provides an example scenario to illustrate the benefits of using an A2A (Agent2Agent) protocol for complex interactions between AI agents.

🌐 中文：本节通过一个示例场景，说明在 AI 智能体之间发生复杂交互时，使用 A2A（Agent2Agent）协议的好处。

#### 💬 A User's Complex Request ／ 用户的复杂请求

A user interacts with an AI assistant, giving it a complex prompt like "Plan an international trip."

🌐 中文：用户与一个 AI 助手交互，给出一个像"规划一次国际旅行"这样的复杂指令。

![User's complex request flow: User → Prompt → AI Assistant](local-file:///Users/ylgeeker/workspace/os-artificer/ebooks/notes/images/domain-papers/mermaid-user-complex-request.png)

🌐 图示：用户发起复杂请求，AI Assistant 接收处理。

#### 🔗 The Need for Collaboration ／ 协作的必要性

The AI assistant receives the prompt and realizes it needs to call upon multiple specialized agents to fulfill the request. These agents include a Flight Booking Agent, a Hotel Reservation Agent, a Currency Conversion Agent, and a Local Tours Agent.

🌐 中文：AI 助手收到指令后意识到，它需要调用多个专精型智能体才能完成请求，包括：机票预订智能体、酒店预订智能体、货币兑换智能体和本地游玩智能体。

![The AI Assistant orchestrates four specialised agents: Flight Booking, Hotel Reservation, Currency Conversion, Local Tours](local-file:///Users/ylgeeker/workspace/os-artificer/ebooks/notes/images/domain-papers/mermaid-need-for-collaboration.png)

🌐 图示：AI Assistant 作为编排者，调用四个专精智能体（机票预订 / 酒店预订 / 货币兑换 / 本地游玩）。

#### 🚧 The Interoperability Challenge ／ 互操作性挑战

The core problem: The agents are unable to work together because each has its own bespoke development and deployment.

The consequence of a lack of a standardized protocol is that these agents cannot collaborate with each other let alone discover what they can do. The individual agents (Flight, Hotel, Currency, and Tours) are isolated.

🌐 中文：核心问题：这些智能体无法协同工作，因为各自都是定制的开发与部署。

缺乏标准化协议的结果是，这些智能体不仅无法彼此协作，连"能做什么"都无从发现。各个智能体（机票、酒店、货币、游玩）彼此孤立。

#### ✅ The "With A2A" Solution ／ "有 A2A"的解决方案

The A2A Protocol provides standard methods and data structures for agents to communicate with one another, regardless of their underlying implementation, so the same agents can be used as an interconnected system, communicating seamlessly through the standardized protocol.

The AI assistant, now acting as an orchestrator, receives the cohesive information from all the A2A-enabled agents. It then presents a single, complete travel plan as a seamless response to the user's initial prompt.

🌐 中文：A2A 协议为智能体之间的通信提供了标准方法和数据结构，与其底层实现无关。于是同样的这些智能体可以作为一个互联系统，通过标准化协议无缝通信。

此时作为"编排者"的 AI 助手，接收了所有支持 A2A 的智能体给出的整合信息，再向用户最初的那条指令返回一个完整、统一的旅行计划。

![A2A 角色关系：User、A2A Client（客户端智能体）、A2A Server（远程智能体）](local-file:///Users/ylgeeker/workspace/os-artificer/ebooks/notes/images/domain-papers/a2a-actors.png)

### 🌟 Core Benefits of A2A ／ A2A 的核心优势

Implementing the A2A protocol offers significant advantages across the AI ecosystem:

🌐 中文：落地 A2A 协议能为整个 AI 生态带来显著优势：

- **Secure collaboration**: Without a standard, it's difficult to ensure secure communication between agents. A2A uses HTTPS for secure communication and maintains opaque operations, so agents can't see the inner workings of other agents during collaboration.  
  🌐 中文：没有标准，很难保证智能体间的通信安全。A2A 用 HTTPS 做安全通信，并保持"不透明运行"，协作时智能体看不到对方的内部运作。
- **Interoperability**: A2A breaks down silos between different AI agent ecosystems, enabling agents from various vendors and frameworks to work together seamlessly.  
  🌐 中文：A2A 打破不同 AI 智能体生态之间的孤岛，让来自不同厂商和框架的智能体无缝协作。
- **Agent autonomy**: A2A allows agents to retain their individual capabilities and act as autonomous entities while collaborating with other agents.  
  🌐 中文：A2A 让智能体在协作时保留自身能力，作为自主实体行动。
- **Reduced integration complexity**: The protocol standardizes agent communication, enabling teams to focus on the unique value their agents provide.  
  🌐 中文：协议标准化了智能体通信，让团队聚焦在自己的智能体提供的独特价值上。
- **Support for LRO**: The protocol supports long-running operations (LRO) and streaming with Server-Sent Events (SSE) and asynchronous execution.  
  🌐 中文：协议支持长时运行操作（LRO），以及通过 Server-Sent Events（SSE）的流式传输和异步执行。

### 🏛️ Key Design Principles of A2A ／ A2A 的关键设计原则

A2A development follows principles that prioritize broad adoption, enterprise-grade capabilities, and future-proofing.

🌐 中文：A2A 的开发遵循一系列原则，优先保障广泛采用、企业级能力与面向未来：

- **Simplicity**: A2A leverages existing standards like HTTP, JSON-RPC, and Server-Sent Events (SSE). This avoids reinventing core technologies and accelerates developer adoption.  
  🌐 中文：A2A 复用 HTTP、JSON-RPC、SSE 等既有标准，避免重新发明核心技术，加速开发者采用。
- **Enterprise Readiness**: A2A addresses critical enterprise needs. It aligns with standard web practices for robust authentication, authorization, security, privacy, tracing, and monitoring.  
  🌐 中文：A2A 回应关键的企业需求，在强大的认证、授权、安全、隐私、链路追踪与监控上对齐标准 Web 实践。
- **Asynchronous**: A2A natively supports long-running tasks. It handles scenarios where agents or users might not remain continuously connected. It uses mechanisms like streaming and push notifications.  
  🌐 中文：A2A 原生支持长时任务，处理智能体或用户可能不持续在线的场景，借助流式传输和推送通知等机制。
- **Modality Independent**: The protocol allows agents to communicate using a wide variety of content types. This enables rich and flexible interactions beyond plain text.  
  🌐 中文：协议允许智能体用多种多样的内容类型通信，支持超越纯文本的丰富灵活交互。
- **Opaque Execution**: Agents collaborate effectively without exposing their internal logic, memory, or proprietary tools. Interactions rely on declared capabilities and exchanged context. This preserves intellectual property and enhances security.  
  🌐 中文：智能体无需暴露内部逻辑、记忆或专有工具即可有效协作，交互依赖"声明的能力"和"交换的上下文"，从而保护知识产权、增强安全性。

### 🧱 Understanding the Agent Stack: A2A, MCP, Agent Frameworks and Models ／ 理解智能体技术栈

A2A is situated within a broader agent stack, which includes:

🌐 中文：A2A 位于一个更庞大的智能体技术栈之中，这个技术栈包含：

- **A2A**: Standardizes communication among agents deployed in different organizations and developed using diverse frameworks.  
  🌐 中文：标准化那些部署在不同组织、用不同框架开发的智能体之间的通信。
- **MCP**: Connects models to data and external resources.  
  🌐 中文：把模型连接到数据与外部资源。
- **Frameworks (like ADK)**: Provide toolkits for constructing agents.  
  🌐 中文：提供构建智能体的工具包。
- **Models**: Fundamental to an agent's reasoning, these can be any Large Language Model (LLM).  
  🌐 中文：智能体推理的基础，可以是任何大语言模型（LLM）。

![A2A 与 MCP 在技术栈中的位置](local-file:///Users/ylgeeker/workspace/os-artificer/ebooks/notes/images/domain-papers/agentic-stack.png)

#### 🔗 A2A and MCP ／ A2A 与 MCP

In the broader ecosystem of AI communication, you might be familiar with protocols designed to facilitate interactions between agents, models, and tools. Notably, the Model Context Protocol (MCP) is an emerging standard focused on connecting Large Language Models (LLMs) with data and external resources.

🌐 中文：在更广阔的 AI 通信生态中，你可能熟悉那些用于打通智能体、模型与工具的协议。值得注意的是，Model Context Protocol（MCP）是一个新兴标准，聚焦于把大语言模型（LLM）连接到数据与外部资源。

The Agent2Agent (A2A) protocol is designed to standardize communication between AI agents, particularly those deployed in external systems. A2A is positioned to complement MCP, addressing a distinct yet related aspect of agent interaction.

🌐 中文：A2A（Agent2Agent）协议则旨在标准化 AI 智能体之间的通信，尤其是那些部署在外部系统中的智能体。A2A 定位为对 MCP 的补充，解决智能体交互中一个不同但相关的侧面。

- **MCP's Focus:** Reducing the complexity involved in connecting agents with tools and data. Tools are typically stateless and perform specific, predefined functions (e.g., a calculator, a database query).  
  🌐 中文：降低把智能体连接到工具与数据的复杂度。工具通常是无状态的，执行特定的预设功能（如计算器、数据库查询）。
- **A2A's Focus:** Enabling agents to collaborate within their native modalities, allowing them to communicate as agents (or as users) rather than being constrained to tool-like interactions. This enables complex, multi-turn interactions where agents reason, plan, and delegate tasks to other agents. For example, this facilitates multi-turn interactions, such as those involving negotiation or clarification when placing an order.  
  🌐 中文：让智能体以其原生模态协作，以"智能体（或用户）"的身份通信，而不是被限制在"类工具"的交互里。这支持复杂的多轮交互，让智能体能推理、规划并把任务委托给其他智能体。例如，它促成了多轮交互，比如下单时的协商或澄清。

![A2A + MCP 协同示意](local-file:///Users/ylgeeker/workspace/os-artificer/ebooks/notes/images/domain-papers/a2a-mcp-readme.png)

The practice of encapsulating an agent as a simple tool is fundamentally limiting, as it fails to capture the agent's full capabilities. This critical distinction is explored in the post, [Why Agents Are Not Tools](https://discuss.google.dev/t/agents-are-not-tools/192812). For a more in-depth comparison, refer to the [A2A and MCP Comparison](https://a2a-protocol.org/latest/topics/a2a-and-mcp/) document.

🌐 中文：把智能体封装成一个简单工具，本质上是有局限的，因为它无法体现智能体的全部能力。这一关键区别在《Why Agents Are Not Tools》一文中深入探讨。更深入的对比，请参阅《A2A 与 MCP 对比》文档。

#### 🛠️ A2A and ADK ／ A2A 与 ADK

The [Agent Development Kit (ADK)](https://google.github.io/adk-docs) is an open-source agent development toolkit developed by Google. A2A is a communication protocol for agents that enables inter-agent communication, regardless of the framework used for their construction (e.g., ADK, LangGraph, or Crew AI). ADK is a flexible and modular framework for developing and deploying AI agents. While optimized for Gemini AI and the Google ecosystem, ADK is model-agnostic, deployment-agnostic, and built for compatibility with other frameworks.

🌐 中文：Agent Development Kit（ADK）是 Google 开发的开源智能体开发工具包。A2A 是一套面向智能体的通信协议，无论用哪种框架构建（如 ADK、LangGraph 或 Crew AI），都能实现智能体间通信。

ADK 是一套灵活、模块化的框架，用于开发和部署 AI 智能体。虽然它针对 Gemini AI 和 Google 生态做了优化，但 ADK 是模型无关、部署无关的，并为兼容其他框架而构建。

### 🔄 A2A Request Lifecycle ／ A2A 请求生命周期

The A2A request lifecycle is a sequence that details the four main steps a request follows: agent discovery, authentication, `sendMessage` API, and `sendMessageStream` API. The following diagram provides a deeper look into the operational flow, illustrating the interactions between the client, A2A server, and auth server.

🌐 中文：A2A 请求生命周期描述了一个请求所经历的四个主要步骤：智能体发现（agent discovery）、认证（authentication）、`sendMessage` API 和 `sendMessageStream` API。下面这张图深入展示了运行流程，呈现了客户端、A2A 服务器与认证服务器之间的交互。

![A2A request lifecycle sequence: 1. Agent Discovery  →  2. Authentication (with optional openIdConnect alt)  →  3. sendMessage API  →  4. sendMessageStream API](local-file:///Users/ylgeeker/workspace/os-artificer/ebooks/notes/images/domain-papers/mermaid-request-lifecycle.png)

🌐 图示：A2A 请求生命周期完整时序——智能体发现 → 认证（支持 openIdConnect 单点登录分支）→ 同步 sendMessage → 流式 sendMessageStream。

---

*本文根据 A2A 官方文档（a2a-protocol.org）翻译整理，供技术学习存档之用。*
