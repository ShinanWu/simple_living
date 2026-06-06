FROM node:20-alpine AS builder
WORKDIR /app
COPY package.json package-lock.json* ./
RUN npm install --registry=https://registry.npmmirror.com --no-fund --no-audit
COPY . .
RUN npm run build

FROM alpine:3.20
RUN apk add --no-cache nginx
COPY --from=builder /app/dist /usr/share/nginx/html/backoffice
COPY nginx.conf /etc/nginx/http.d/default.conf
EXPOSE 8088
CMD ["nginx", "-g", "daemon off;"]
