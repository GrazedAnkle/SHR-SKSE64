/*
 * This file is part of SHR.
 *
 * SHR is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, version 3.
 *
 * SHR is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * SHR. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

namespace SHR
{
    class InputHandler : public RE::BSTEventSink<RE::InputEvent *>
    {
    public:
        static InputHandler &GetInstance()
        {
            static InputHandler s_Instance;
            return s_Instance;
        }

        static void Register();

        static const std::atomic_int &IsListening();

        RE::BSEventNotifyControl ProcessEvent(
            RE::InputEvent *const *event,
            RE::BSTEventSource<RE::InputEvent *> *source
        ) override;
    };
}
