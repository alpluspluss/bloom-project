import {defineConfig} from 'vitepress'

// https://vitepress.dev/reference/site-config
export default defineConfig({
    title: "Bloom",
    description: "A small, type-safe compiler backend in C++20",
    base: '/bloom/',
    themeConfig: {
        // https://vitepress.dev/reference/default-theme-config
        nav: [
            {text: 'Home', link: '/'},
            {text: 'Getting Started', link: '/getting-started'}
        ],

        sidebar: [
            {
                items: [
                    {text: 'Home', link: '/'},
                    {text: 'Getting Started with Bloom', link: '/getting-started'},
                ]
            },
        ],

        socialLinks: [
            {icon: 'github', link: 'https://github.com/alpluspluss/bloom'}
        ]
    }
})
