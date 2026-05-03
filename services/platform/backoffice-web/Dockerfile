FROM node:20-alpine AS builder
WORKDIR /app
COPY package.json package-lock.json* ./
RUN npm install --registry=https://registry.npmmirror.com --no-fund --no-audit
COPY . .
RUN npm run build

FROM node:20-alpine
WORKDIR /app
COPY --from=builder /app/dist /app/dist
RUN npm install -g serve --registry=https://registry.npmmirror.com --no-fund --no-audit
EXPOSE 8088
CMD ["serve", "-s", "/app/dist", "-l", "8088"]
