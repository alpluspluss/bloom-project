---
# https://vitepress.dev/reference/default-theme-home-page
layout: home

hero:
  name: "Bloom"
  tagline: "A small, type-safe compiler backend in C++20"
  image:
    src: /logo.png
    alt: Bloom logo
  actions:
    - theme: brand
      text: Get Started
      link: /getting-started
      
    - theme: alt
      text: View on GitHub
      link: https://github.com/alpluspluss/bloom

features:
  - title: Graph-based IR
    details: Directed graph structure with hierarchical regions to represent both data flow and control flow.
    
  - title: Easy Integration
    details: Designed to be comprehensible and approachable while maintaining performance and efficiency.

  - title: Optimization Infrastructure
    details: Framework for analysis and transformation passes including DCE, PRE, and alias analysis.
---
