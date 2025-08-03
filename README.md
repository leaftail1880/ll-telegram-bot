# LeviLamina TelegramBot mod

Connect your minecraft chat with telegram chat using telegram bot

## Usage

1. Install using lip `lip install github.com/leaftail1880/ll-telegram-bot`
2. Start your server
3. Create telegram bot in t.me/botfather
4. Edit plugins/TelegramBot/config.json

## Config Documentation

Example:

- player `leftail1880` sent message `hello` in minecraft chat

  1. in server console appears `leaftail1880: hello` using minecraft.sendToConsoleLogFormat
  2. in telegram chat appears `leaftail1880: hello` using telegram.sendToChatFormat

- user `leaftail1880` sent message `hello` in telegram chat

  1. in server console appears `Telegram leaftail1880: hello` using telegram.sendToConsoleLogFormat
  2. in minecraft chat appears `Telegram leaftail1880: hello` using minecraft.sendToChatFormat

- user `leaftail1880` left
  1. in telegram chat appears `-leaftail1880` using telegram.leaveTextFormat

Pending...

## Development

For detailed instructions, see the [LeviLamina Documentation](https://lamina.levimc.org/developer_guides/tutorials/create_your_first_mod/)

1. Clone the repository
2. Run `xmake project -k compile_commands` in the root of the repository
3. Run `xmake f -m debug` in the root of the repository
4. Run `xmake` to build the mod.

After a successful build, you will find mod in `bin/`

## Contributing

Ask questions by creating an issue.

PRs accepted.

## License

CC0-1.0 © Leaftail1880
