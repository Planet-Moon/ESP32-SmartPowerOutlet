/** @type {import('tailwindcss').Config} */
module.exports = {
  content: [
    "./LocalServer/templates/**/*.html",
    "./LocalServer/static/**/*.js",
    "./node_modules/flowbite/**/*.js"
  ],
  theme: {
    extend: {},
  },
  plugins: [
    require('flowbite/plugin'),
    require('prettier-plugin-tailwindcss')
  ],
}
