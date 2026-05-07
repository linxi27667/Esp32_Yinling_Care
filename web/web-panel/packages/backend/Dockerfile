FROM node:20-alpine

WORKDIR /app

# 安装 tsx 用于运行 TypeScript
RUN npm install -g tsx

COPY package*.json ./
RUN npm install --production

COPY . .

EXPOSE 3001

ENV NODE_ENV=production
ENV PORT=3001
ENV MQTT_BROKER=mqtt://broker.emqx.io:1883

CMD ["tsx", "src/index.ts"]
