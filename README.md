# LeviLamina TelegramBot mod

Connect your minecraft chat with telegram chat using telegram bot

## Usage

1. Install using lip `lip install github.com/leaftail1880/ll-telegram-bot`
2. Start your server
3. Create telegram bot in t.me/botfather
4. Edit plugins/TelegramBot/config.json

## Config Documentation

For formatting, see https://core.telegram.org/bots/api#formatting-options

Example:

- player `leftail1880` sent message `hello` in minecraft chat

  1. in server console appears `leaftail1880: hello` using minecraft.consoleLogFormat
  2. in telegram chat appears `leaftail1880: hello` using telegram.chatFormat

- user `leaftail1880` sent message `hello` in telegram chat

  1. in server console appears `Telegram leaftail1880: hello` using telegram.consoleLogFormat
  2. in minecraft chat appears `Telegram leaftail1880: hello` using minecraft.chatFormat

- user `leaftail1880` left
  1. in telegram chat appears `-leaftail1880` using telegram.leaveTextFormat

For chat format you can use the following placeholders:
{sourceName} - .source from the config the message is coming from
{name} - short name. nameTag for minecraft and first name + last name for telegram
{username} - user tag from minecraft or username from telegram. Fallbacks to name if empty for telegram
{message} - Chat content

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
