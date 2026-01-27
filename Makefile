COMP = c++
FLAGS = -Wall -Wextra -Werror -std=c++98

NAME = ircserv

SRCS = main.cpp \
		ServerRead.cpp \
		IRCParser.cpp \
		Server.cpp \
		ServerBasicCommands.cpp \
		ServerChannel.cpp \
		ServerChannelOperator.cpp \
		ServerDispatcher.cpp \
		Mode.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(COMP) $(FLAGS) $(OBJS) -o $(NAME)

%.o: %.cpp
	$(COMP) $(FLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re