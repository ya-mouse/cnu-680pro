# Put definitions for platforms and boards in here.

ifdef CONFIG_BOARD_GENERIC
PLATFORM := h8300h
BOARD := generic
endif

ifdef CONFIG_BOARD_AKI3068NET
PLATFORM := h8300h
BOARD := aki3068net
endif

ifdef CONFIG_BOARD_H8MAX
PLATFORM := h8300h
BOARD := h8max
endif

export PLATFORM
export BOARD
