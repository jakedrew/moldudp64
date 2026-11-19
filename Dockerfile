FROM alpine:3.20 AS build
RUN apk add --no-cache g++ make
WORKDIR /app

COPY . .
RUN make

# -------------------------
FROM alpine:3.20
RUN apk add --no-cache libstdc++
WORKDIR /app

COPY --from=build /app/send /app/recv /app/start.sh ./

EXPOSE 8080
CMD ["sh", "./start.sh"]
